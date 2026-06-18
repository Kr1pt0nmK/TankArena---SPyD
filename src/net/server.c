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
    char       ip[IP_MAX];     /* IP del cliente (para migracion de host) */
    thread_t   thr;
    volatile int used;
    Server    *sv;
} ClientSlot;

struct Server {
    GameState *gs;
    mutex_t   *glock;          /* protege gs */
    sock_t     listen;
    uint16_t   port;
    volatile int running;
    thread_t   accept_thr;
    thread_t   sim_thr;
    mutex_t    clock;          /* protege la lista de clientes */
    ClientSlot cl[MAX_PLAYERS];
    chat_handler on_chat;      /* entrega los chats recibidos a la GUI del host */
    void        *chat_user;
};

/* Difunde a todos los clientes la lista de jugadores con su IP. Asi, si el host
   cae, los supervivientes saben a quien reconectarse (migracion de host). */
static void broadcast_peers(Server *s)
{
    PeerInfo peers[MAX_PLAYERS];
    uint8_t buf[MAX_FRAME];
    mutex_lock(&s->clock);
    int n = 0;
    for (int i = 0; i < MAX_PLAYERS; i++) {
        if (!s->cl[i].used) continue;
        peers[n].id = s->cl[i].id;
        snprintf(peers[n].ip, IP_MAX, "%s", s->cl[i].ip);
        n++;
    }
    int len = enc_peers(buf, peers, n);
    for (int i = 0; i < MAX_PLAYERS; i++)
        if (s->cl[i].used) frame_send(s->cl[i].s, MSG_PEERS, buf, (uint16_t)len);
    mutex_unlock(&s->clock);
}

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
            /* reenvia el chat a todos los clientes (general). El filtrado por
               equipo se hara cuando exista el modelo de equipos. */
            mutex_lock(&s->clock);
            for (int i = 0; i < MAX_PLAYERS; i++)
                if (s->cl[i].used) frame_send(s->cl[i].s, MSG_CHAT, pl, (uint16_t)plen);
            mutex_unlock(&s->clock);
            /* y lo muestra tambien en la GUI del host */
            if (s->on_chat) {
                int sender, channel; char text[CHAT_MAX + 1];
                dec_chat(pl, plen, &sender, &channel, text, sizeof(text));
                s->on_chat(sender, channel, text, s->chat_user);
            }
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

    broadcast_peers(s);
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

        char ip[IP_MAX] = "0.0.0.0";
        net_peer_ip(c, ip, sizeof(ip));

        mutex_lock(&s->clock);
        s->cl[id].s = c; s->cl[id].id = id; s->cl[id].sv = s;
        snprintf(s->cl[id].ip, IP_MAX, "%s", ip);
        s->cl[id].used = 1;
        thread_create(&s->cl[id].thr, client_loop, &s->cl[id]);
        mutex_unlock(&s->clock);

        broadcast_peers(s);
        fprintf(stderr, "[host] cliente %s conectado como jugador %d\n", ip, id);
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
    s->gs = gs; s->glock = lock; s->port = port; s->running = 1;
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

    /* Despierta el hilo de accept: en Linux, cerrar el socket de escucha no
       siempre desbloquea un accept() en curso. Abrimos una conexion ficticia a
       nosotros mismos para que accept() retorne y el hilo vea running == 0.
       (En Windows closesocket si lo desbloquea, pero esto funciona en ambos.) */
    sock_t waker = net_connect("127.0.0.1", s->port);
    if (waker != SOCK_INVALID) net_close(waker);
    net_close(s->listen);

    mutex_lock(&s->clock);
    for (int i = 0; i < MAX_PLAYERS; i++)
        if (s->cl[i].used) net_close(s->cl[i].s);  /* desbloquea los recv */
    mutex_unlock(&s->clock);

    thread_join(s->accept_thr);
    thread_join(s->sim_thr);
    mutex_destroy(&s->clock);
    free(s);
}

void server_set_chat_handler(Server *s, chat_handler cb, void *user)
{
    if (!s) return;
    s->on_chat = cb;
    s->chat_user = user;
}

void server_send_chat(Server *s, const char *text)
{
    if (!s || !text || !text[0]) return;

    mutex_lock(s->glock);
    int sender = s->gs->local_id;     /* el jugador local del host */
    mutex_unlock(s->glock);

    uint8_t pl[3 + CHAT_MAX];
    int n = enc_chat(pl, sender, CHAT_GENERAL, text);

    mutex_lock(&s->clock);
    for (int i = 0; i < MAX_PLAYERS; i++)
        if (s->cl[i].used) frame_send(s->cl[i].s, MSG_CHAT, pl, (uint16_t)n);
    mutex_unlock(&s->clock);

    if (s->on_chat) s->on_chat(sender, CHAT_GENERAL, text, s->chat_user);
}
