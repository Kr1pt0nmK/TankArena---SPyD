#ifndef SERVER_H
#define SERVER_H

#include "game.h"
#include "thread.h"
#include <stdint.h>

/* Host autoritativo. Simula el juego en su propio hilo y difunde el estado a
   los clientes. El GameState y su mutex los provee quien llama (la GUI):
   - el host escribe su propio input en gs->players[local] (protegido por lock),
   - el hilo de red del server escribe el input de los clientes,
   - el hilo de simulacion llama game_update y difunde STATE. */

typedef struct Server Server;

/* gs debe estar inicializado (game_init) y con el jugador local ya agregado. */
Server *server_start(GameState *gs, mutex_t *lock, uint16_t port);
void    server_stop(Server *s);

#endif /* SERVER_H */
