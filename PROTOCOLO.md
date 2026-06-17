# Protocolo de red de Tank Arena

Protocolo propio del equipo, sobre **TCP**. Diseñado para que una app de **Linux**
y una de **Windows** se comuniquen sin problemas (independiente de plataforma).

## Principios

- **Orden de bytes de red** (big-endian) para todos los campos multibyte.
- **Nunca** se envían `struct` crudos (el *padding* difiere entre compiladores/SO).
- Coordenadas y ángulos viajan como **enteros escalados**, no como `float`/`double`
  (los floats no son portables byte a byte).
- Cada mensaje es un *frame* con **cabecera de longitud fija**.

## Formato del frame

```
[u16 len][u16 type][payload de (len-2) bytes]
```

- `len`  = 2 (del campo type) + tamaño del payload.
- `type` = tipo de mensaje (ver tabla).

## Arquitectura: host autoritativo

Un nodo es el **host** (servidor): simula todo el juego y difunde el estado.
Los demás son **clientes**: envían su input y dibujan lo que reciben. Solo el host
calcula colisiones y daño → nunca hay dos versiones distintas del mundo.

## Tipos de mensaje

| Tipo | Valor | Dirección | Contenido |
|---|---|---|---|
| `HELLO`   | 1 | cliente → host | saludo inicial (1 byte de versión) |
| `WELCOME` | 2 | host → cliente | `u8 id` (slot de jugador asignado, 0–3) |
| `INPUT`   | 3 | cliente → host | teclas + ángulo de torreta |
| `STATE`   | 4 | host → cliente | snapshot del mundo (~60 Hz) |
| `CHAT`    | 5 | ambos | texto (general/equipo) — *reservado para la rama chat* |
| `PING`/`PONG` | 6/7 | ambos | *reservado* (latencia / keepalive) |

### INPUT (payload = 3 bytes)

```
u8  teclas   (bit0=arriba, bit1=abajo, bit2=izq, bit3=der, bit4=disparo)
i16 aim      (ángulo de torreta en radianes * 1000)
```

### STATE (payload variable)

```
u32 tick
i16 shake (intensidad de sacudida * 100)

u8  n_jugadores
  por jugador:
    u8  id
    u8  flags   (bit0 = vivo)
    i16 x, i16 y                (píxeles)
    i16 body_angle, i16 turret  (radianes * 1000)
    i16 hp
    u16 bajas

u8  n_enemigos
  por enemigo: u8 vivo, i16 x, i16 y, i16 turret

u8  n_balas
  por bala: i16 x, i16 y, u8 owner   (0–3 = jugador, 255 = enemigo)

u8  n_explosiones
  por explosión: i16 x, i16 y, u8 t, u8 maxt, u8 escala*10
```

## Handshake

```
cliente → HELLO
host    → WELCOME(id)
... luego, en bucle:
cliente → INPUT   (cada frame)
host    → STATE   (~60 Hz)
```

## Desconexión

Si `recv` devuelve 0 (cerrado) o error, el host libera el slot del jugador y la
partida continúa; el cliente marca la conexión como caída. Nadie crashea.
