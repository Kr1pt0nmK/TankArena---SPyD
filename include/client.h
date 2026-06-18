#ifndef CLIENT_H
#define CLIENT_H

#include "game.h"
#include "thread.h"
#include "protocol.h"   /* chat_handler */
#include <stdint.h>

/* Cliente: se conecta al host, recibe snapshots en un hilo de red y los vuelca
   al GameState (protegido por lock). Envia su input al host. No simula. */

typedef struct Client Client;

Client *client_connect(const char *host, uint16_t port,
                       GameState *gs, mutex_t *lock, int *local_id);
int     client_send_input(Client *c, const Input *in);  /* 0 ok, -1 error */
int     client_send_chat(Client *c, const char *text);  /* 0 ok, -1 error */
int     client_alive(Client *c);                        /* 1 conectado, 0 caido */
int     client_id(Client *c);                           /* id de jugador asignado */
int     client_get_peers(Client *c, PeerInfo *out, int max); /* lista de jugadores; devuelve count */
void    client_close(Client *c);

/* Registra quien recibe los chats que llegan del host (para mostrarlos en la GUI). */
void    client_set_chat_handler(Client *c, chat_handler cb, void *user);

#endif /* CLIENT_H */
