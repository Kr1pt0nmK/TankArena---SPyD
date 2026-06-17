#ifndef CLIENT_H
#define CLIENT_H

#include "game.h"
#include "thread.h"
#include <stdint.h>

/* Cliente: se conecta al host, recibe snapshots en un hilo de red y los vuelca
   al GameState (protegido por lock). Envia su input al host. No simula. */

typedef struct Client Client;

Client *client_connect(const char *host, uint16_t port,
                       GameState *gs, mutex_t *lock, int *local_id);
int     client_send_input(Client *c, const Input *in);  /* 0 ok, -1 error */
int     client_alive(Client *c);                        /* 1 conectado, 0 caido */
void    client_close(Client *c);

#endif /* CLIENT_H */
