# Guión del video — Tank Arena (15-20 min)

Video del Examen Ordinario (criterio 4, 25%): explicar **en código y con pruebas** los
**tres primeros puntos** de la rúbrica, con los **cuatro integrantes a cuadro**.
Subir a YouTube como **no listado o público**.

**Estructura:** los puntos clave del código repartidos para que cada integrante salga a
cuadro. Total ≈ 19 min.

| Parte | Quién | Tema | Tiempo |
|---|---|---|---|
| 0 | Todos | Intro | 1.5 min |
| 1 | Integrante 1 | Punto 1: multiplataforma, C + APIs nativas | 4 min |
| 2 | Integrante 2 | Punto 2: Cliente/Servidor P2P + sockets | 4.5 min |
| 3 | Integrante 3 | Punto 3: multihilo + mutex (sección crítica) | 4.5 min |
| 4 | Integrante 4 | Prueba en vivo (demo) | 3.5 min |
| 5 | Todos | Cierre | 1 min |

---

## Parte 0 — Intro (1.5 min) · los 4 a cuadro

**Decir:**
> "Hola, somos [nombres], del equipo [X] de Sistemas de Cómputo Paralelo y Distribuido.
> Presentamos **Tank Arena**, un videojuego de tanques multijugador con arquitectura
> **Cliente/Servidor Punto a Punto**, escrito en **C**, que corre nativo en **Linux y
> Windows** y sincroniza el paralelismo con **mutex**. En este video explicamos el código
> y probamos los tres primeros puntos de la rúbrica."

**Mostrar:** el juego corriendo unos segundos (un tanque moviéndose) como gancho.

---

## Parte 1 — Punto 1: Un solo código en C, nativo en ambos SO (4 min) · Integrante 1

**Idea central:** un solo código base en C que compila nativo en Windows y Linux usando
directivas de preprocesador. Las diferencias entre SO se aíslan en una capa de abstracción.

**Mostrar y decir:**

1. **Estructura del proyecto** (`CMakeLists.txt`) — "Usamos CMake; el mismo build en los dos SO."
2. **Abstracción de sockets** (`src/platform/net.c`):
   > "Aquí está el `#ifdef _WIN32`. En Windows usamos la API nativa **Winsock** —
   > `WSAStartup`, `closesocket` — y en Linux los **sockets POSIX** con `close`. El resto del
   > programa llama a `net_listen`, `net_connect`, etc., sin saber en qué SO está."
   - Señalar `net_init()` y `net_close()`.
3. **Abstracción de hilos/mutex** (`src/platform/thread.c`):
   > "Lo mismo con los hilos: en Windows usamos la API nativa **Win32** — `_beginthreadex` y
   > `CRITICAL_SECTION` — y en Linux **pthreads** con `pthread_mutex_t`. Una sola interfaz:
   > `thread_create`, `mutex_lock`…"
   - Mostrar el contraste Win32 vs POSIX.
4. **Cierre del punto:**
   > "Gracias a esto, fuera de estos dos archivos **no hay un solo `#ifdef`**. Es C estándar y
   > APIs nativas de cada sistema."

**Tip:** muestra rápido el juego compilando en Windows (`cmake --build build`) para evidenciar
que es nativo.

---

## Parte 2 — Punto 2: Cliente/Servidor P2P con sockets (4.5 min) · Integrante 2

**Idea central:** arquitectura de host autoritativo: un nodo simula todo y difunde el estado;
los demás envían su input y dibujan. Comunicación por TCP con protocolo propio.

**Mostrar y decir:**

1. **El protocolo** (`include/protocol.h`):
   > "Definimos un protocolo propio. Cada mensaje es un *frame*: `[longitud][tipo][datos]`.
   > Usamos **orden de bytes de red** y **nunca** enviamos structs crudos, porque el *padding*
   > difiere entre Windows y Linux. Así una app de Linux y una de Windows se entienden —
   > conectividad heterogénea."
   - Mostrar los tipos: `HELLO`, `WELCOME`, `INPUT`, `STATE`, `CHAT`.
2. **Serialización** (`src/net/protocol.c`):
   > "Los enteros se escriben byte a byte en big-endian (`pu16`, `pu32`), y los ángulos como
   > enteros escalados. Portable byte a byte."
3. **El handshake** (`src/net/client.c` y `src/net/server.c`):
   > "El cliente se conecta y manda **HELLO**; el host le responde **WELCOME** con su número de
   > jugador."
   - Cliente: `client_connect` (envía HELLO, recibe WELCOME).
   - Host: `accept_loop` asigna un slot libre y manda WELCOME.
4. **El bucle:**
   > "Luego, cada frame el cliente manda su **INPUT** y el host difunde el **STATE** del mundo a
   > ~60 Hz. Solo el host calcula colisiones, así que nunca hay dos versiones del mundo."

**Tip:** abre `ipconfig`/IP y enseña el comando `tank-arena host` y `tank-arena client <ip>`
para aterrizar que son dos apps por red.

---

## Parte 3 — Punto 3: Multihilo + mutex (sección crítica) (4.5 min) · Integrante 3

**Idea central:** varios hilos trabajando en paralelo, y un mutex protege el estado compartido
(patrón productor-consumidor). Este es el punto más evaluado.

**Mostrar y decir:**

1. **Los hilos del host** (`src/net/server.c`):
   > "El host corre varios hilos en paralelo:
   > - un hilo **`accept_loop`** que acepta conexiones,
   > - un hilo **`client_loop` por cada cliente** que recibe sus inputs (productores),
   > - y un hilo **`sim_loop`** que a 60 Hz actualiza el juego y difunde el estado (consumidor)."
2. **La sección crítica** (lo más importante):
   > "El `GameState` es compartido: lo escriben los hilos de red y lo lee/actualiza el hilo de
   > simulación. Para que no haya **condiciones de carrera**, lo protegemos con un mutex, `glock`."
   - Mostrar en `client_loop`: `mutex_lock(s->glock)` … `players[id].in = in` … `mutex_unlock`
     (productor escribe).
   - Mostrar en `sim_loop`: `mutex_lock(s->glock)` … `game_update` + `enc_state` …
     `mutex_unlock` (consumidor).
   > "Solo un hilo entra a la sección crítica a la vez. Hay un segundo mutex, `clock`, que
   > protege la lista de clientes."
3. **El cliente y la GUI** (`src/main.c`):
   > "En el cliente, un hilo de red recibe el estado y otro hilo —el de GTK— dibuja. Como
   > **GTK no es thread-safe**, el chat que llega por la red se entrega al hilo gráfico con
   > `g_idle_add`, nunca tocando los widgets directamente."
   - Mostrar `on_draw` con su `mutex_lock(&a->lock)`.
4. **Cierre del punto:**
   > "Así demostramos paralelismo real sincronizado con exclusión mutua, igual en Windows
   > (CRITICAL_SECTION) y en Linux (pthread_mutex)."

---

## Parte 4 — Prueba en vivo / demo (3.5 min) · Integrante 4

**Objetivo:** probar (no solo explicar) que los 3 puntos funcionan.

**Hacer en pantalla:**

1. **Compilar** en Windows (`cmake --build build`) → "compila sin errores, en C nativo"
   *(prueba punto 1)*.
2. **Lanzar host y cliente** (en 2 PCs reales si pueden, o en la misma con loopback):
   - Mover el tanque en una ventana → se mueve en la otra → "el estado viaja por sockets y se
     sincroniza" *(prueba punto 2)*.
   - Escribir en el **chat** → llega a la otra *(refuerza red + hilos)*.
3. **Mostrar la consola** del host: `[host] cliente conectado como jugador 1` → evidencia de los
   hilos aceptando clientes *(prueba punto 3)*.
4. **(Opcional fuerte)** correr **`nettest`** → `NETTEST PASS` → "prueba automática de red,
   protocolo y mutex".
5. **Cerrar un cliente** → el host sigue vivo → "manejo de desconexión sin crashear".

> Si tienen una PC Linux y una Windows hablando entre sí, **grábenlo** — eso demuestra la
> conectividad heterogénea y sube el punto 1 y 2.

---

## Parte 5 — Cierre (1 min) · los 4 a cuadro

**Decir:**
> "En resumen: un solo código en C con APIs nativas de cada SO, arquitectura Cliente/Servidor
> punto a punto con protocolo propio sobre sockets, y paralelismo multihilo sincronizado con
> mutex. Gracias por ver. Equipo [X]."

---

## Consejos para cumplir la rúbrica al 100%

- **Mínimo 15 min**: si se quedan cortos, alarguen las Partes 2 y 3 (son las de más peso, 50%).
- **Los 4 deben salir a cuadro** — repártanse hablar, no solo aparecer.
- **"Explicando en código"**: muestren el editor con el archivo abierto mientras hablan, no solo
  el juego.
- **"y prueba"**: la Parte 4 es obligatoria — tienen que demostrarlo corriendo, no solo contarlo.
- Súbanlo a YouTube como **no listado o público** y guarden el enlace para el QR del cartel.
