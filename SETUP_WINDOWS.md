# 🪟 Guía de instalación en Windows (GTK 3 + Glade en C) — desde cero

Esta guía deja tu PC con Windows lista para **compilar y correr** el proyecto Tank Arena
(escrito en C con GTK 3). Sigue los pasos **en orden**. Si te atoras, busca en internet el
mensaje de error exacto o avísale al equipo.

Usaremos **MSYS2 + MinGW-w64**, que es la forma **oficialmente recomendada por GTK** para
Windows. Compila C **nativo** en Windows (cumple el requisito del proyecto).

> ℹ️ Si el profesor exige específicamente el IDE *Visual Studio* (el del icono azul), avísale al
> equipo: ese es otro camino (vcpkg) mucho más latoso. Esta guía NO usa Visual Studio, usa MSYS2,
> que es más confiable para GTK.

---

## Paso 1 — Descargar e instalar MSYS2

1. Entra a **https://www.msys2.org**
2. Descarga el instalador (`msys2-x86_64-XXXXXXXX.exe`).
3. Ejecútalo. Deja la ruta de instalación por defecto: **`C:\msys64`**.
4. Termina la instalación con las opciones por defecto.

---

## Paso 2 — Abrir la terminal CORRECTA (¡esto confunde a todos!)

MSYS2 instala **varias** terminales. Tienes que usar **siempre la misma**:

➡️ En el menú de inicio busca y abre **"MSYS2 MINGW64"** (icono **rosa/morado**).

⚠️ **NO** uses la que dice solo "MSYS2 MSYS" (icono violeta) — esa NO sirve para compilar
programas de Windows. Tiene que decir **MINGW64**.

---

## Paso 3 — Actualizar MSYS2

En la terminal **MINGW64**, ejecuta:

```bash
pacman -Syu
```

- Te preguntará confirmación: escribe `Y` y Enter.
- **Es normal que al final cierre la terminal.** Si lo hace, vuelve a abrir "MSYS2 MINGW64"
  y ejecuta otra vez, para terminar de actualizar:

```bash
pacman -Su
```

---

## Paso 4 — Instalar el compilador, GTK, Glade y CMake

Copia y pega estos comandos uno por uno (escribe `Y` + Enter cuando pregunte):

```bash
pacman -S --needed base-devel mingw-w64-x86_64-toolchain
```

```bash
pacman -S mingw-w64-x86_64-gtk3 mingw-w64-x86_64-glade
```

```bash
pacman -S mingw-w64-x86_64-cmake mingw-w64-x86_64-ninja mingw-w64-x86_64-pkgconf
```

Esto instala: el compilador `gcc`, la librería **GTK 3**, el diseñador visual **Glade**,
**CMake** y **Ninja** (para compilar).

---

## Paso 5 — Verificar que todo quedó bien

En la misma terminal MINGW64, ejecuta cada línea. Debe imprimir una versión, no un error:

```bash
gcc --version
```
```bash
cmake --version
```
```bash
pkg-config --modversion gtk+-3.0
```

Si las tres imprimen un número de versión → **¡listo, tu entorno funciona!** 🎉

Si quieres probar que GTK abre ventanas, ejecuta el diseñador de interfaces:

```bash
glade
```

(Debe abrirse la aplicación Glade.)

---

## Paso 6 — Obtener el código del proyecto

> Haz esto cuando el equipo ya tenga el repositorio del proyecto.

Dentro de la terminal MINGW64, las unidades de Windows se acceden así: `C:\` es `/c/`.

Ejemplo, si el proyecto está en `C:\Users\TuNombre\Juego`:

```bash
cd /c/Users/TuNombre/Juego
```

(Si usan Git: `git clone <url-del-repo>` y luego `cd` a la carpeta.)

---

## Paso 7 — Compilar el proyecto

Desde la carpeta del proyecto, en la terminal MINGW64:

```bash
cmake -B build -G Ninja
```
```bash
cmake --build build
```

Si compila sin errores, se generará el ejecutable dentro de la carpeta `build/`.

---

## Paso 8 — Ejecutar

```bash
./build/tank-arena.exe
```

⚠️ **Importante:** ejecuta el programa **desde la terminal MINGW64**. Si haces doble clic en
el `.exe` desde el Explorador de Windows, dará error de "falta una DLL" porque no encuentra las
librerías de GTK. (Para entregar el `.exe` por separado, después hay que empaquetar las DLLs,
pero eso es para el final del proyecto.)

---

## ❓ Problemas comunes

| Problema | Solución |
|---|---|
| `pkg-config: command not found` | Falta el Paso 4 (instala `mingw-w64-x86_64-pkgconf`). |
| `gtk/gtk.h: No such file or directory` | No instalaste GTK (Paso 4) o estás en la terminal equivocada (usa **MINGW64**, Paso 2). |
| `cmake: command not found` | Falta `mingw-w64-x86_64-cmake` (Paso 4). |
| Al ejecutar el `.exe`: "falta XXXX.dll" | Lo estás corriendo fuera de la terminal MINGW64 (ver Paso 8). |
| Comandos no encontrados aunque instalaste | Casi siempre es que abriste "MSYS2 **MSYS**" en vez de "MSYS2 **MINGW64**". |

> 💡 Si algo falla, busca en internet el comando que ejecutaste + el error completo,
> mencionando que usas **MSYS2 MINGW64 en Windows con GTK 3**. Así encuentras respuestas que aplican.
