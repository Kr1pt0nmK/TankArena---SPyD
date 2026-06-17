/* Prueba automatica de la capa de red + protocolo + hilos, sin GUI.
   Levanta un servidor en un hilo, conecta un cliente por loopback, intercambia
   un INPUT y un STATE, y verifica que todo llegue intacto. Devuelve 0 si pasa. */

#include "net.h"
#include "thread.h"
#include "protocol.h"
#include "game.h"
#include <stdio.h>
#include <string.h>

#define TEST_PORT 51999

static volatile int g_srv_ok = 0;

static void *srv_thread(void *arg)
{
    (void)arg;
    sock_t ls = net_listen(TEST_PORT);
    if (ls == SOCK_INVALID) return NULL;

    sock_t c = net_accept(ls);
    if (c == SOCK_INVALID) { net_close(ls); return NULL; }

    uint16_t t; uint8_t pl[MAX_FRAME]; int pn;

    /* HELLO */
    if (frame_recv(c, &t, pl, sizeof(pl), &pn) <= 0) { net_close(c); net_close(ls); return NULL; }

    /* INPUT */
    if (frame_recv(c, &t, pl, sizeof(pl), &pn) <= 0) { net_close(c); net_close(ls); return NULL; }
    Input in;
    dec_input(pl, pn, &in);
    g_srv_ok = (t == MSG_INPUT && in.up && in.fire && !in.left);

    /* responde STATE */
    GameState gs;
    game_init(&gs);
    game_add_player(&gs, 0);
    game_add_player(&gs, 2);
    gs.ticks = 12345;
    uint8_t buf[MAX_FRAME];
    int n = enc_state(buf, &gs);
    frame_send(c, MSG_STATE, buf, (uint16_t)n);

    net_close(c);
    net_close(ls);
    return NULL;
}

int main(void)
{
    net_init();

    thread_t th;
    thread_create(&th, srv_thread, NULL);
    sleep_ms(250);   /* deja que el servidor escuche */

    sock_t s = net_connect("127.0.0.1", TEST_PORT);
    if (s == SOCK_INVALID) { printf("NETTEST FAIL: no conecta\n"); return 1; }

    uint8_t h[1] = { 1 };
    frame_send(s, MSG_HELLO, h, 1);

    Input in; memset(&in, 0, sizeof(in));
    in.up = true; in.fire = true; in.aim = 1.5;
    uint8_t ib[8];
    int il = enc_input(ib, &in);
    frame_send(s, MSG_INPUT, ib, (uint16_t)il);

    uint16_t t; uint8_t pl[MAX_FRAME]; int pn;
    int r = frame_recv(s, &t, pl, sizeof(pl), &pn);
    int ok = (r == 1 && t == MSG_STATE);

    GameState gs;
    game_init(&gs);
    dec_state(pl, pn, &gs);
    ok = ok && gs.ticks == 12345 &&
         gs.players[0].active && gs.players[2].active && !gs.players[1].active;

    net_close(s);
    thread_join(th);
    net_cleanup();

    if (ok && g_srv_ok) {
        printf("NETTEST PASS: tick=%lu, jugadores 0 y 2 activos, input recibido OK\n",
               gs.ticks);
        return 0;
    }
    printf("NETTEST FAIL: ok=%d srv_ok=%d (tick=%lu)\n", ok, g_srv_ok, gs.ticks);
    return 1;
}
