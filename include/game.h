#ifndef GAME_H
#define GAME_H

#include <stdbool.h>
#include <stdint.h>

/* ---- Dimensiones del mundo (en tiles y pixeles) ---- */
#define MAP_W 26
#define MAP_H 18
#define TILE  40
#define NAME_MAX 16   /* nombre del jugador (incluye el '\0') */
#define VIEW_W (MAP_W * TILE)
#define VIEW_H (MAP_H * TILE)

#define MAX_PLAYERS   4
#define MAX_ENEMIES   3
#define MAX_BULLETS   64
#define MAX_PARTICLES 320
#define MAX_BLASTS    16

#define TANK_R   15.0   /* radio de colision del tanque */
#define BULLET_R 4.0

/* Entrada de un jugador (lo que el cliente envia y el host aplica). */
typedef struct {
    bool   up, down, left, right, fire;
    double aim;          /* angulo de torreta en radianes */
} Input;

/* Un tanque: jugador o enemigo. */
typedef struct {
    double x, y;
    double body_angle;    /* radianes; 0 = mira a la derecha (+x) */
    double turret_angle;
    double cr, cg, cb;     /* color del chasis */
    bool   active;         /* slot en uso (para jugadores) */
    bool   alive;          /* vivo en combate */
    int    hp;             /* 0..100 */
    int    score;          /* bajas */
    double muzzle;         /* timer del fogonazo */
    int    fire_cd;        /* cooldown de disparo (jugadores) */
    int    fire_timer;     /* cuenta atras de disparo (enemigos) */
    int    respawn;        /* cuenta atras de reaparicion (enemigos) */
    bool   on_foot;        /* soldado a pie: bajo del tanque al morir */
    bool   eliminated;     /* fuera de la ronda: lo mataron a pie */
    char   name[NAME_MAX]; /* nombre elegido por el jugador */
    Input  in;             /* input actual (jugadores) */
} Tank;

typedef struct {
    double x, y, vx, vy;
    int    life;
    bool   active;
    int    owner;          /* 0..3 = jugador; -1 = enemigo */
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

/* Estado completo del juego. Es lo que el mutex protege: lo escribe el hilo de
   simulacion (host) o el hilo de red (cliente), y lo lee el hilo de la GUI. */
typedef struct {
    Tank     players[MAX_PLAYERS];
    Tank     enemies[MAX_ENEMIES];
    int      enemy_count;

    Bullet   bullets[MAX_BULLETS];
    Particle particles[MAX_PARTICLES];
    Blast    blasts[MAX_BLASTS];

    const int *map;        /* MAP_W*MAP_H, 1 = pared */

    double   shake;
    unsigned long ticks;

    int      local_id;     /* que jugador controlo (para HUD/apuntado); -1 si ninguno */

    /* ---- Ronda (todos contra todos: ultimo tanque en pie) ---- */
    int      round_over;   /* 1 = ronda terminada, mostrando ganador */
    int      round_winner; /* id del jugador ganador, -1 si ninguno */
    int      round_timer;  /* cuenta atras para reiniciar la ronda */
} GameState;

void game_init(GameState *gs);
void game_add_player(GameState *gs, int id);    /* activa un slot de jugador */
void game_remove_player(GameState *gs, int id); /* lo desactiva (desconexion) */
void game_update(GameState *gs);
bool game_is_wall(const GameState *gs, double px, double py);

/* Colores (los usa el cliente al reconstruir el estado recibido). */
void game_set_player_visual(Tank *t, int id);
void game_set_enemy_visual(Tank *t);

#endif /* GAME_H */
