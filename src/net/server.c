#include "server.h"
#include "net.h"
#include "protocol.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

typedef struct Server Server;

/* Un cliente conectado (ocupa el mismo indice que su id de jugador). */
typedef struct {
    sock_t     s;
    int        id;
    thread_t   thr;
    volatile int used;
    Server    *sv;
} ClientSlot;

struct Server {
    GameState *gs;
    mutex_t   *glock;          /* protege gs */
    sock_t     listen;
    volatile int running;
    thread_t   accept_thr;
    thread_t   sim_thr;
    mutex_t    clock;          /* protege la lista de clientes */
    ClientSlot cl[MAX_PLAYERS];
};

/* Hilo por cliente: recibe sus INPUT y los vuelca al estado (productor). */
static void *client_loop(void *arg)
{
    ClientSlot *cs = (ClientSlot *)arg;
    Server *s = cs->sv;
    int id = cs->id;
    uint16_t type;
    uint8_t pl[MAX_FRAME];
    int plen;

    while (s->running) {
        int r = frame_recv(cs->s, &type, pl, sizeof(pl), &plen);
        if (r <= 0) break;

        if (type == MSG_INPUT) {
            Input in;
            dec_input(pl, plen, &in);
            mutex_lock(s->glock);
            s->gs->players[id].in = in;
            mutex_unlock(s->glock);
        } else if (type == MSG_CHAT) {
            /* reenvia el chat a todos (general). El filtrado por equipo va en la rama chat. */
            mutex_lock(&s->clock);
            for (int i = 0; i < MAX_PLAYERS; i++)
                if (s->cl[i].used) frame_send(s->cl[i].s, MSG_CHAT, pl, (uint16_t)plen);
            mutex_unlock(&s->clock);
        }
    }

    /* desconexion */
    mutex_lock(s->glock);
    game_remove_player(s->gs, id);
    mutex_unlock(s->glock);

    mutex_lock(&s->clock);
    cs->used = 0;
    net_close(cs->s);
    mutex_unlock(&s->clock);

    fprintf(stderr, "[host] jugador %d desconectado\n", id);
    return NULL;
}

/* Hilo que acepta conexiones y asigna un slot de jugador a cada una. */
static void *accept_loop(void *arg)
{
    Server *s = (Server *)arg;
    while (s->running) {
        sock_t c = net_accept(s->listen);
        if (c == SOCK_INVALID) {
            if (!s->running) break;
            continue;
        }

        /* saludo del cliente */
        uint16_t type; uint8_t pl[MAX_FRAME]; int plen;
        if (frame_recv(c, &type, pl, sizeof(pl), &plen) <= 0) { net_close(c); continue; }

        /* asigna primer slot de jugador libre */
        mutex_lock(s->glock);
        int id = -1;
        for (int i = 0; i < MAX_PLAYERS; i++)
            if (!s->gs->players[i].active) { id = i; break; }
        if (id >= 0) game_add_player(s->gs, id);
        mutex_unlock(s->glock);

        if (id < 0) { net_close(c); continue; }   /* servidor lleno */

        uint8_t w[1] = { (uint8_t)id };
        if (frame_send(c, MSG_WELCOME, w, 1) != 0) {
            mutex_lock(s->glock); game_remove_player(s->gs, id); mutex_unlock(s->glock);
            net_close(c);
            continue;
        }

        mutex_lock(&s->clock);
        s->cl[id].s = c; s->cl[id].id = id; s->cl[id].sv = s; s->cl[id].used = 1;
        thread_create(&s->cl[id].thr, client_loop, &s->cl[id]);
        mutex_unlock(&s->clock);

        fprintf(stderr, "[host] cliente conectado como jugador %d\n", id);
    }
    return NULL;
}

/* Hilo de simulacion: ~60 Hz, avanza el mundo y difunde el estado (consumidor). */
static void *sim_loop(void *arg)
{
    Server *s = (Server *)arg;
    uint8_t buf[MAX_FRAME];
    while (s->running) {
        mutex_lock(s->glock);
        game_update(s->gs);
        int n = enc_state(buf, s->gs);
        mutex_unlock(s->glock);

        mutex_lock(&s->clock);
        for (int i = 0; i < MAX_PLAYERS; i++)
            if (s->cl[i].used)
                frame_send(s->cl[i].s, MSG_STATE, buf, (uint16_t)n);
        mutex_unlock(&s->clock);

        sleep_ms(16);
    }
    return NULL;
}

Server *server_start(GameState *gs, mutex_t *lock, uint16_t port)
{
    Server *s = (Server *)calloc(1, sizeof(*s));
    if (!s) return NULL;
    s->gs = gs; s->glock = lock; s->running = 1;
    mutex_init(&s->clock);

    s->listen = net_listen(port);
    if (s->listen == SOCK_INVALID) { mutex_destroy(&s->clock); free(s); return NULL; }

    thread_create(&s->accept_thr, accept_loop, s);
    thread_create(&s->sim_thr, sim_loop, s);
    fprintf(stderr, "[host] escuchando en el puerto %u\n", (unsigned)port);
    return s;
}

void server_stop(Server *s)
{
    if (!s) return;
    s->running = 0;
    net_close(s->listen);              /* desbloquea accept */

    mutex_lock(&s->clock);
    for (int i = 0; i < MAX_PLAYERS; i++)
        if (s->cl[i].used) net_close(s->cl[i].s);  /* desbloquea los recv */
    mutex_unlock(&s->clock);

    thread_join(s->accept_thr);
    thread_join(s->sim_thr);
    mutex_destroy(&s->clock);
    free(s);
}
