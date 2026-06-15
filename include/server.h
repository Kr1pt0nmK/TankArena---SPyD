#ifndef SERVER_H
#define SERVER_H

#include "game.h"
#include "thread.h"
#include "protocol.h"   /* chat_handler */
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

/* Registra quien recibe los chats que llegan al host (para mostrarlos en su GUI). */
void    server_set_chat_handler(Server *s, chat_handler cb, void *user);
/* El host envia un chat: lo difunde a los clientes y lo entrega a su propia GUI. */
void    server_send_chat(Server *s, const char *text);

#endif /* SERVER_H */
