#ifndef PROTOCOL_H
#define PROTOCOL_H

/* Protocolo propio de Tank Arena.

   Marco (frame) en el cable:
       [u16 len][u16 type][payload de (len-2) bytes]
   - len y type van en orden de bytes de red (big-endian).
   - NUNCA se envian structs crudos (el padding difiere entre SO/compilador).
   - Coordenadas y angulos se mandan como enteros escalados, no como floats.

   Tipos de mensaje:
       HELLO   cliente -> host : saludo inicial
       WELCOME host -> cliente : asigna id de jugador  [u8 id]
       INPUT   cliente -> host : teclas + angulo de torreta
       STATE   host -> cliente : snapshot del mundo (~60 Hz)
       CHAT    ambos           : texto (general / equipo)  [reservado]
*/

#include <stdint.h>
#include "game.h"
#include "net.h"

enum {
    MSG_HELLO   = 1,
    MSG_WELCOME = 2,
    MSG_INPUT   = 3,
    MSG_STATE   = 4,
    MSG_CHAT    = 5,
    MSG_PING    = 6,
    MSG_PONG    = 7
};

#define PROTO_PORT 50505
#define MAX_FRAME  4096

/* Framing sobre el socket. */
int frame_send(sock_t s, uint16_t type, const uint8_t *payload, uint16_t plen);
int frame_recv(sock_t s, uint16_t *type, uint8_t *payload, int max, int *plen);
/* frame_send: 0 ok, -1 error. frame_recv: 1 ok, 0 cerrado, -1 error. */

/* Serializacion de mensajes (devuelven longitud del payload en encode). */
int  enc_input(uint8_t *out, const Input *in);
void dec_input(const uint8_t *p, int plen, Input *in);
int  enc_state(uint8_t *out, const GameState *gs);
void dec_state(const uint8_t *p, int plen, GameState *gs);

/* ---------- CHAT ---------- */
#define CHAT_MAX 200                       /* maximo de bytes de texto por mensaje */
enum { CHAT_GENERAL = 0, CHAT_TEAM = 1 };  /* canales (TEAM queda para mas adelante) */

/* Serializa un chat. Formato: [u8 emisor][u8 canal][u8 len][texto de len bytes].
   Devuelve la longitud del payload. */
int  enc_chat(uint8_t *out, int sender, int channel, const char *text);

/* Lee un chat. Copia el texto (terminado en '\0') en text, hasta maxlen-1 bytes.
   Devuelve la longitud del texto (0 si el frame es invalido). */
int  dec_chat(const uint8_t *p, int plen, int *sender, int *channel,
              char *text, int maxlen);

/* Callback que entrega a la GUI un chat recibido. OJO: lo invoca el hilo de red,
   asi que no debe tocar GTK directamente (debe marshalear con g_idle_add). */
typedef void (*chat_handler)(int sender, int channel, const char *text, void *user);

#endif /* PROTOCOL_H */
