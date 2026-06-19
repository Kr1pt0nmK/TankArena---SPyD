# Guión del video — Tank Arena (15-20 min)

Video del Examen Ordinario (criterio 4, 25%): explicar **en código y con pruebas** los
**tres primeros puntos** de la rúbrica, con los **cuatro integrantes a cuadro**.
Subir a YouTube como **no listado o público** (guardar el enlace para el QR del cartel).

**Equipo (UNISTMO · Dr. Jesús Arellano Pimentel):**
Denilson Alexis Rosado Cortez · Jared López Toledo · Isacar Jiménez Charis · Gerson Antonio Regalado López

> Roles sugeridos (ajústenlos a quién domina cada tema):

| Parte | Quién | Tema | Tiempo |
|---|---|---|---|
| 0 | Todos | Intro | 1.5 min |
| 1 | Denilson | Punto 1: multiplataforma, C + APIs nativas | 4 min |
| 2 | Jared | Punto 2: Cliente/Servidor Punto a Punto + sockets | 4 min |
| 3 | Isacar | Punto 3: multihilo + mutex (sección crítica) | 4.5 min |
| 4 | Gerson + todos | Prueba en vivo (demo entre varias PCs) | 5 min |
| 5 | Todos | Cierre | 1 min |

---

## ✅ CHECKLIST antes de grabar (háganlo ya, estando juntos)

- [ ] Cada PC **compiló** el juego (`cmake --build build`). Tener al menos **una Linux y una Windows**.
- [ ] **Todas en el mismo WiFi.** Confirmar IP del host (`hostname -I` en Linux / `ipconfig` en Windows).
- [ ] El **firewall** del host permite el puerto **50505** (en Windows, "Permitir acceso").
- [ ] Cada quien puso su **nombre y color** en el menú de configuración (se ven sobre el tanque y en el chat).
- [ ] Probar 30 s que todos conectan **antes** de grabar.
- [ ] Tener abierto el **editor de código** (VS Code) para mostrar los archivos al explicar.

---

## Parte 0 — Intro (1.5 min) · los 4 a cuadro

> "Hola, somos Denilson, Jared, Isacar y Gerson, de la Universidad del Istmo, materia Sistemas
> de Cómputo Paralelo y Distribuido con el Dr. Arellano. Presentamos **Tank Arena**, un videojuego
> de tanques multijugador (hasta 4 jugadores) con arquitectura **Cliente/Servidor Punto a Punto**,
> escrito en **C**, que corre nativo en **Linux y Windows**, se comunica por **sockets** y
> sincroniza el paralelismo con **mutex**. En este video explicamos el código y probamos los tres
> primeros puntos de la rúbrica."

**Mostrar:** el juego corriendo unos segundos (tanques moviéndose, disparos, sonido) como gancho.

---

## Parte 1 — Punto 1: Un solo código en C, nativo en ambos SO (4 min) · Denilson

**Idea central:** un solo código base en C que compila nativo en Windows y Linux usando directivas
de preprocesador. Las diferencias entre SO se aíslan en una capa de abstracción.

**Mostrar y decir:**

1. **Estructura + build** (`CMakeLists.txt`): "Usamos CMake; el **mismo** build genera el ejecutable
   nativo en los dos SO. En Windows compilamos con MinGW, en Linux con GCC."
2. **Abstracción de sockets** (`src/platform/net.c`):
   > "Aquí está el `#ifdef _WIN32`. En Windows usamos la API nativa **Winsock** (`WSAStartup`,
   > `closesocket`) y en Linux **sockets POSIX** (`close`). El resto del programa llama a
   > `net_listen`, `net_connect`… sin saber en qué SO está."
3. **Abstracción de hilos/mutex** (`src/platform/thread.c`):
   > "Lo mismo con hilos: Windows usa **Win32** (`_beginthreadex`, `CRITICAL_SECTION`) y Linux
   > **pthreads** (`pthread_mutex_t`). Una sola interfaz: `thread_create`, `mutex_lock`…"
4. **Audio también multiplataforma** (`src/audio.c`): "El sonido usa miniaudio, una librería de una
   sola cabecera que funciona igual en ambos SO." *(refuerza C + portable)*
5. **Cierre:**
   > "Fuera de estos archivos de la capa de plataforma **no hay un solo `#ifdef`**. Es C estándar
   > con APIs nativas de cada sistema."

**Tip:** muestren el juego **compilando en Windows** (`cmake --build build`) en vivo → evidencia de que es nativo.

---

## Parte 2 — Punto 2: Cliente/Servidor Punto a Punto con sockets (4 min) · Jared

**Idea central:** arquitectura de host autoritativo con conexiones punto a punto por TCP y protocolo
propio. Cada app trae **cliente y servidor**, y si el host cae, otro nodo toma el relevo (migración).

**Mostrar y decir:**

1. **Protocolo propio** (`include/protocol.h`):
   > "Cada mensaje es un *frame*: `[longitud][tipo][datos]`, en **orden de bytes de red**, y
   > **nunca** enviamos structs crudos porque el *padding* difiere entre Windows y Linux. Por eso
   > una app Linux y una Windows se entienden: **conectividad heterogénea**."
   - Tipos: `HELLO`, `WELCOME`, `INPUT`, `STATE`, `CHAT`, `PEERS`, `PROFILE`.
2. **Serialización** (`src/net/protocol.c`): "Enteros byte a byte en big-endian (`pu16`/`pu32`),
   ángulos como enteros escalados. Portable byte a byte."
3. **Handshake y bucle** (`client.c` / `server.c`):
   > "El cliente manda **HELLO**, el host responde **WELCOME** con su número de jugador. Luego cada
   > frame el cliente manda **INPUT** y el host difunde el **STATE** a ~60 Hz. Solo el host calcula
   > colisiones → nunca hay dos versiones del mundo. Cada conexión TCP es un **enlace punto a punto**."
4. **Punto a punto / migración** (lo que sube este punto a 10):
   > "Cada app compila cliente **y** servidor. Con el mensaje **PEERS**, el host reparte las IPs de
   > todos. Si el host se cae, los supervivientes **eligen** al de id más bajo como nuevo host y se
   > reconectan solos. O sea, cualquier nodo puede ser servidor: es punto a punto real."

**Tip:** enseñen los comandos `tank-arena host` y `tank-arena client <ip>` para aterrizar que son dos apps por red.

---

## Parte 3 — Punto 3: Multihilo + mutex (sección crítica) (4.5 min) · Isacar

**Idea central:** varios hilos en paralelo y un mutex protege el estado compartido (productor-
consumidor). **Es el punto de más peso, explíquenlo con calma.**

**Mostrar y decir:**

1. **Los hilos del host** (`src/net/server.c`):
   > "El host corre varios hilos a la vez:
   > - **`accept_loop`**: acepta conexiones,
   > - **`client_loop` por cada cliente**: recibe sus inputs (productores),
   > - **`sim_loop`**: a 60 Hz actualiza el juego y difunde el estado (consumidor)."
2. **La sección crítica** (lo más importante):
   > "El `GameState` es compartido: lo escriben los hilos de red y lo lee el de simulación. Para
   > evitar **condiciones de carrera** lo protegemos con el mutex `glock`."
   - En `client_loop`: `mutex_lock(glock)` … `players[id].in = in` … `mutex_unlock` *(productor)*.
   - En `sim_loop`: `mutex_lock(glock)` … `game_update` + `enc_state` … `mutex_unlock` *(consumidor)*.
   > "Solo un hilo entra a la sección crítica a la vez. Un segundo mutex, `clock`, protege la lista de clientes."
3. **Cliente + GUI** (`src/main.c`):
   > "En el cliente, un hilo de red recibe el estado y el hilo de GTK dibuja. Como **GTK no es
   > thread-safe**, lo que llega por red (chat) se entrega al hilo gráfico con `g_idle_add`, nunca
   > tocando widgets desde otro hilo." — mostrar `on_draw` con su `mutex_lock(&a->lock)`.
4. **Cierre:**
   > "Paralelismo real sincronizado con exclusión mutua, igual en Windows (`CRITICAL_SECTION`) y en
   > Linux (`pthread_mutex`)."

---

## Parte 4 — Prueba en vivo / demo (5 min) · Gerson + todos

**Objetivo:** PROBAR (no solo explicar) que los 3 puntos funcionan. **Aprovechen que están juntos.**

**Hacer en pantalla (graben varias pantallas a la vez si pueden):**

1. **Compilar en Windows** (`cmake --build build`) → "compila sin errores, C nativo" *(prueba punto 1)*.
2. **Partida entre varias PCs — al menos una Linux y una Windows** *(prueba puntos 1 y 2: heterogéneo)*:
   - Uno hace `host`, los demás `client <ip-del-host>`.
   - Se ven los **tanques con nombre y color** de cada quien moviéndose en todas las pantallas.
   - Disparar, explosiones, **sonido** → "el estado viaja por sockets y se sincroniza".
3. **Chat** entre las máquinas → el mensaje llega a todas con el nombre/color del jugador *(red + hilos)*.
4. **Consola del host**: mostrar `[host] cliente <ip> conectado como jugador N` → evidencia de los
   hilos aceptando clientes *(prueba punto 3)*.
5. **⭐ Migración de host (lo que más impresiona):** **cierren la ventana del host** → en la consola de
   otro jugador aparece `[migracion] ahora soy el HOST` y **la partida continúa** → "tolerancia a
   fallos: cualquier nodo puede ser servidor".
6. **(Opcional fuerte)** `./build/nettest` → `NETTEST PASS` → "prueba automática de red, protocolo y mutex".

> La toma de **Linux y Windows jugando juntos** es la más valiosa: demuestra conectividad heterogénea
> y sube los puntos 1 y 2. ¡No se les olvide grabarla!

---

## Parte 5 — Cierre (1 min) · los 4 a cuadro

> "En resumen: un solo código en **C** con **APIs nativas** de cada SO, arquitectura
> **Cliente/Servidor punto a punto** con protocolo propio sobre **sockets**, y **paralelismo
> multihilo sincronizado con mutex**. Además: hasta 4 jugadores, chat, sonido y migración de host.
> Gracias por ver. Equipo de UNISTMO."

---

## Consejos para cumplir la rúbrica al 100%

- **Mínimo 15 min**: si se quedan cortos, alarguen las Partes 2 y 3 (más peso) y la demo.
- **Los 4 deben salir a cuadro** y **hablar**, no solo aparecer.
- **"Explicando en código"**: muestren el editor con el archivo abierto mientras hablan.
- **"y prueba"**: la Parte 4 es obligatoria — demuéstrenlo corriendo, sobre todo **Linux↔Windows**.
- Súbanlo a YouTube **no listado o público** y guarden el enlace para el **QR del cartel**.
