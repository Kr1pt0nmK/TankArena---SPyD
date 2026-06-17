#include "client.h"
#include "net.h"
#include "protocol.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

struct Client {
    sock_t     s;
    GameState *gs;
    mutex_t   *lock;
    int        local_id;
    volatile int alive;
    thread_t   thr;
    chat_handler on_chat;    /* entrega los chats recibidos a la GUI */
    void        *chat_user;
};

/* Hilo de red: recibe STATE y reconstruye el estado (productor). */
static void *recv_loop(void *arg)
{
    Client *c = (Client *)arg;
    uint16_t type;
    uint8_t pl[MAX_FRAME];
    int plen;

    while (c->alive) {
        int r = frame_recv(c->s, &type, pl, sizeof(pl), &plen);
        if (r <= 0) break;

        if (type == MSG_STATE) {
            mutex_lock(c->lock);
            dec_state(pl, plen, c->gs);
            mutex_unlock(c->lock);
        } else if (type == MSG_CHAT) {
            if (c->on_chat) {
                int sender, channel; char text[CHAT_MAX + 1];
                dec_chat(pl, plen, &sender, &channel, text, sizeof(text));
                c->on_chat(sender, channel, text, c->chat_user);
            }
        }
    }
    c->alive = 0;
    fprintf(stderr, "[cliente] conexion con el host terminada\n");
    return NULL;
}

Client *client_connect(const char *host, uint16_t port,
                       GameState *gs, mutex_t *lock, int *local_id)
{
    sock_t s = net_connect(host, port);
    if (s == SOCK_INVALID) return NULL;

    uint8_t h[1] = { 1 };
    if (frame_send(s, MSG_HELLO, h, 1) != 0) { net_close(s); return NULL; }

    uint16_t type; uint8_t pl[MAX_FRAME]; int plen;
    if (frame_recv(s, &type, pl, sizeof(pl), &plen) <= 0 ||
        type != MSG_WELCOME || plen < 1) {
        net_close(s);
        return NULL;
    }

    Client *c = (Client *)calloc(1, sizeof(*c));
    if (!c) { net_close(s); return NULL; }
    c->s = s; c->gs = gs; c->lock = lock;
    c->local_id = pl[0];
    c->alive = 1;
    if (local_id) *local_id = c->local_id;

    thread_create(&c->thr, recv_loop, c);
    fprintf(stderr, "[cliente] conectado como jugador %d\n", c->local_id);
    return c;
}

int client_send_input(Client *c, const Input *in)
{
    if (!c || !c->alive) return -1;
    uint8_t buf[8];
    int n = enc_input(buf, in);
    if (frame_send(c->s, MSG_INPUT, buf, (uint16_t)n) != 0) {
        c->alive = 0;
        return -1;
    }
    return 0;
}

int client_send_chat(Client *c, const char *text)
{
    if (!c || !c->alive || !text || !text[0]) return -1;
    uint8_t pl[3 + CHAT_MAX];
    int n = enc_chat(pl, c->local_id, CHAT_GENERAL, text);
    if (frame_send(c->s, MSG_CHAT, pl, (uint16_t)n) != 0) {
        c->alive = 0;
        return -1;
    }
    return 0;
}

void client_set_chat_handler(Client *c, chat_handler cb, void *user)
{
    if (!c) return;
    c->on_chat = cb;
    c->chat_user = user;
}

int client_alive(Client *c) { return c && c->alive; }

void client_close(Client *c)
{
    if (!c) return;
    c->alive = 0;
    net_close(c->s);
    thread_join(c->thr);
    free(c);
}
