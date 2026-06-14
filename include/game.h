#ifndef GAME_H
#define GAME_H

#include <stdbool.h>

/* ---- Dimensiones del mundo (en tiles y pixeles) ---- */
#define MAP_W 20
#define MAP_H 15
#define TILE  40
#define VIEW_W (MAP_W * TILE)
#define VIEW_H (MAP_H * TILE)

#define MAX_ENEMIES   3
#define MAX_BULLETS   64
#define MAX_PARTICLES 320
#define MAX_BLASTS    16

#define TANK_R   15.0   /* radio de colision del tanque */
#define BULLET_R 4.0

/* Un tanque: posicion, angulo del chasis y de la torreta, estado de combate. */
typedef struct {
    double x, y;
    double body_angle;    /* radianes; 0 = mira a la derecha (+x) */
    double turret_angle;
    double cr, cg, cb;     /* color del chasis */
    bool   alive;
    double muzzle;         /* timer del fogonazo */
    int    fire_timer;     /* cuenta atras de disparo (enemigos) */
    int    respawn;        /* cuenta atras de reaparicion */
} Tank;

typedef struct {
    double x, y, vx, vy;
    int    life;
    bool   active;
    bool   from_player;
} Bullet;

typedef struct {
    double x, y, vx, vy;
    double life, maxlife;
    double size;
    double r, g, b;
    bool   active;
} Particle;

/* Onda expansiva de una explosion. */
typedef struct {
    double x, y;
    double t, maxt;
    double scale;
    bool   active;
} Blast;

/* Estado completo del juego (lo que mas adelante protegera el mutex). */
typedef struct {
    Tank player;
    Tank enemies[MAX_ENEMIES];
    int  enemy_count;

    Bullet   bullets[MAX_BULLETS];
    Particle particles[MAX_PARTICLES];
    Blast    blasts[MAX_BLASTS];

    const int *map;        /* MAP_W*MAP_H, 1 = pared */

    double mouse_x, mouse_y;
    bool   key_up, key_down, key_left, key_right;
    bool   fire_kb, fire_mouse;

    int    player_fire_cd;
    int    player_hp;       /* 0..100 */
    int    score;           /* bajas */
    double shake;

    unsigned long ticks;
} GameState;

void game_init(GameState *gs);
void game_update(GameState *gs);
bool game_is_wall(const GameState *gs, double px, double py);

#endif /* GAME_H */
