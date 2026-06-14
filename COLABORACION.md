# 👥 Guía para colaborar (Windows + Linux)

Cómo clonar, compilar y trabajar en equipo sobre este proyecto desde cualquier sistema operativo.
El **mismo código y el mismo CMake** compilan en Windows y en Linux: nadie reescribe nada.

---

## 1️⃣ Setup inicial — cada quien, UNA sola vez

### 🐧 En Linux

Instalar herramientas:

```bash
# Arch / Manjaro
sudo pacman -S --needed base-devel cmake gtk3 glade git

# Ubuntu / Debian
sudo apt install build-essential cmake libgtk-3-dev glade git
```

Clonar y compilar:

```bash
git clone https://github.com/Kr1pt0nmK/TankArena---SPyD.git
cd TankArena---SPyD
cmake -B build
cmake --build build
./build/tank-arena
```

### 🪟 En Windows

1. **Primero** sigue [SETUP_WINDOWS.md](SETUP_WINDOWS.md) para instalar MSYS2 + GTK.
2. Abre la terminal **"MSYS2 MINGW64"** y ejecuta:

```bash
git clone https://github.com/Kr1pt0nmK/TankArena---SPyD.git
cd TankArena---SPyD
cmake -B build -G Ninja
cmake --build build
./build/tank-arena.exe
```

> ⚠️ En Windows, ejecuta el `.exe` **desde la terminal MINGW64** (si no, da error de DLL faltante).

---

## 2️⃣ Flujo de trabajo diario

Para no pisarse entre ustedes, lo mejor es que **cada quien trabaje en su propia rama** y luego
junten todo. El proyecto se divide solito:

| Persona | Rama sugerida | En qué trabaja |
|---|---|---|
| 1 | `red`    | sockets, hilos, mutex, protocolo |
| 2 | `render` | gráficos, animaciones, sonido |
| 3 | `chat`   | chat general / equipos |
| 4 | `ia`     | enemigos, modos de juego |

### Cada vez que te sientas a trabajar:

```bash
# 1. Trae lo último de todos
git checkout main
git pull

# 2. Entra a tu rama
git checkout -b red     # la PRIMERA vez (crea la rama)
git checkout red        # las siguientes veces (entra a tu rama)
git merge main          # trae lo nuevo de main a tu rama

# 3. ...programas y pruebas...

# 4. Guarda tu avance
git add -A
git commit -m "describe lo que hiciste"
git push -u origin red  # sube tu rama
```

### Cuando termines algo y quieras juntarlo con el equipo:

1. Entra al repo en GitHub → aparece el botón **"Compare & pull request"**.
2. Crea el **Pull Request** de tu rama hacia `main`.
3. El equipo lo revisa y le da **"Merge"**.
4. Tu trabajo ya está en `main` para todos.

---

## 3️⃣ Si hay un conflicto (es normal, no asusta)

Pasa cuando dos editan la misma línea. Git marca el archivo así:

```
<<<<<<< HEAD
   tu versión
=======
   la versión del otro
>>>>>>> main
```

Borras las marcas (`<<<`, `===`, `>>>`), dejas el código correcto, y:

```bash
git add archivo-en-conflicto
git commit
```

---

## 4️⃣ Reglas de oro del equipo

1. **`git pull` ANTES de empezar** a programar. Siempre.
2. **Commits chiquitos y seguidos**, no uno gigante al final.
3. La carpeta `build/` **no se sube** (ya está ignorada).
4. **No editen el mismo archivo a la vez** sin avisarse → por eso las ramas.
5. Si algo truena al compilar tras un `pull`, borra `build/` y recompila:
   ```bash
   rm -rf build && cmake -B build && cmake --build build
   ```

---

## 📂 Estructura del proyecto

```
TankArena---SPyD/
├── CMakeLists.txt      # configuración de compilación (Windows + Linux)
├── include/            # cabeceras (.h): estructuras y prototipos
├── src/                # código fuente (.c): lógica, render, main
├── ui/                 # interfaz de Glade (.glade)
├── README.md           # descripción del proyecto
├── SETUP_WINDOWS.md    # instalación en Windows
└── COLABORACION.md     # esta guía
```
