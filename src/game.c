#include "game.h"
#include <math.h>
#include <stdlib.h>
#include <time.h>

#define PLAYER_FIRE_CD 12
#define BULLET_SPEED   6.0
#define BULLET_LIFE    170

static const double PLAYER_START_X = VIEW_W * 0.5;
static const double PLAYER_START_Y = VIEW_H - TILE * 2.5;

/* ---- Mapa de la arena. 1 = pared, 0 = piso. ---- */
static int g_map[MAP_H * MAP_W];

static double frand(void) { return (double)rand() / (double)RAND_MAX; }

static void set_block(int bx, int by, int bw, int bh)
{
    for (int y = by; y < by + bh && y < MAP_H; y++)
        for (int x = bx; x < bx + bw && x < MAP_W; x++)
            g_map[y * MAP_W + x] = 1;
}

static void build_map(void)
{
    for (int y = 0; y < MAP_H; y++)
        for (int x = 0; x < MAP_W; x++)
            g_map[y * MAP_W + x] =
                (x == 0 || y == 0 || x == MAP_W - 1 || y == MAP_H - 1) ? 1 : 0;

    set_block(3, 2, 2, 2);
    set_block(MAP_W - 5, 2, 2, 2);
    set_block(3, MAP_H - 4, 2, 2);
    set_block(MAP_W - 5, MAP_H - 4, 2, 2);
    set_block(9, 5, 2, 5);
    set_block(6, 7, 1, 1);
    set_block(MAP_W - 7, 7, 1, 1);
}

bool game_is_wall(const GameState *gs, double px, double py)
{
    int tx = (int)(px / TILE);
    int ty = (int)(py / TILE);
    if (tx < 0 || ty < 0 || tx >= MAP_W || ty >= MAP_H)
        return true;
    return gs->map[ty * MAP_W + tx] == 1;
}

static bool collision_free(const GameState *gs, double x, double y)
{
    for (int i = 0; i < 8; i++) {
        double a = (M_PI * 2.0 * i) / 8.0;
        if (game_is_wall(gs, x + cos(a) * TANK_R, y + sin(a) * TANK_R))
            return false;
    }
    return true;
}

/* ---------- spawn helpers ---------- */

static void spawn_particle(GameState *gs, double x, double y, double vx, double vy,
                           double life, double size, double r, double g, double b)
{
    for (int i = 0; i < MAX_PARTICLES; i++) {
        Particle *p = &gs->particles[i];
        if (p->active) continue;
        p->x = x; p->y = y; p->vx = vx; p->vy = vy;
        p->life = p->maxlife = life; p->size = size;
        p->r = r; p->g = g; p->b = b; p->active = true;
        return;
    }
}

static void spawn_blast(GameState *gs, double x, double y, double scale)
{
    for (int i = 0; i < MAX_BLASTS; i++) {
        Blast *bl = &gs->blasts[i];
        if (bl->active) continue;
        bl->x = x; bl->y = y; bl->t = 0; bl->maxt = 18; bl->scale = scale;
        bl->active = true;
        break;
    }
    /* chispas */
    int n = (int)(12 * scale);
    for (int i = 0; i < n; i++) {
        double a = frand() * M_PI * 2.0;
        double sp = (1.5 + frand() * 4.0) * scale;
        double tint = 0.6 + frand() * 0.4;
        spawn_particle(gs, x, y, cos(a) * sp, sin(a) * sp,
                       12 + frand() * 16, 2 + frand() * 3,
                       1.0, tint, 0.2 + frand() * 0.2);
    }
    /* humo */
    for (int i = 0; i < 5; i++) {
        double a = frand() * M_PI * 2.0;
        double sp = (0.3 + frand()) * scale;
        double g = 0.35 + frand() * 0.2;
        spawn_particle(gs, x, y, cos(a) * sp, sin(a) * sp - 0.4,
                       26 + frand() * 18, 5 + frand() * 5, g, g, g);
    }
    gs->shake += 5.0 * scale;
    if (gs->shake > 12) gs->shake = 12;
}

static void spawn_bullet(GameState *gs, const Tank *t, bool from_player)
{
    double tipx = t->x + cos(t->turret_angle) * 30.0;
    double tipy = t->y + sin(t->turret_angle) * 30.0;
    for (int i = 0; i < MAX_BULLETS; i++) {
        Bullet *b = &gs->bullets[i];
        if (b->active) continue;
        b->x = tipx; b->y = tipy;
        b->vx = cos(t->turret_angle) * BULLET_SPEED;
        b->vy = sin(t->turret_angle) * BULLET_SPEED;
        b->life = BULLET_LIFE; b->active = true; b->from_player = from_player;
        return;
    }
}

static void place_at_open_tile(GameState *gs, Tank *t)
{
    for (int tries = 0; tries < 40; tries++) {
        int tx = 1 + rand() % (MAP_W - 2);
        int ty = 1 + rand() % (MAP_H - 2);
        if (gs->map[ty * MAP_W + tx] == 1) continue;
        double cx = tx * TILE + TILE / 2.0, cy = ty * TILE + TILE / 2.0;
        if (hypot(cx - gs->player.x, cy - gs->player.y) < 4 * TILE) continue;
        t->x = cx; t->y = cy;
        return;
    }
}

/* ---------- init ---------- */

static void init_enemy(Tank *e)
{
    e->body_angle = e->turret_angle = M_PI / 2.0;
    e->cr = 0.90; e->cg = 0.22; e->cb = 0.27;
    e->alive = true; e->muzzle = 0;
    e->fire_timer = 60 + rand() % 120;
    e->respawn = 0;
}

void game_init(GameState *gs)
{
    srand((unsigned)time(NULL));
    build_map();
    gs->map = g_map;
    gs->ticks = 0;
    gs->key_up = gs->key_down = gs->key_left = gs->key_right = false;
    gs->fire_kb = gs->fire_mouse = false;
    gs->mouse_x = VIEW_W * 0.5;
    gs->mouse_y = VIEW_H * 0.3;
    gs->player_fire_cd = 0;
    gs->player_hp = 100;
    gs->score = 0;
    gs->shake = 0;

    for (int i = 0; i < MAX_BULLETS; i++)   gs->bullets[i].active = false;
    for (int i = 0; i < MAX_PARTICLES; i++) gs->particles[i].active = false;
    for (int i = 0; i < MAX_BLASTS; i++)    gs->blasts[i].active = false;

    gs->player.x = PLAYER_START_X;
    gs->player.y = PLAYER_START_Y;
    gs->player.body_angle = -M_PI / 2.0;
    gs->player.turret_angle = -M_PI / 2.0;
    gs->player.cr = 0.18; gs->player.cg = 0.77; gs->player.cb = 0.71;
    gs->player.alive = true; gs->player.muzzle = 0;
    gs->player.fire_timer = 0; gs->player.respawn = 0;

    gs->enemy_count = MAX_ENEMIES;
    double ex[MAX_ENEMIES] = { TILE * 2.5, VIEW_W - TILE * 2.5, VIEW_W * 0.5 };
    double ey[MAX_ENEMIES] = { TILE * 2.5, TILE * 2.5,          TILE * 3.0 };
    for (int i = 0; i < gs->enemy_count; i++) {
        init_enemy(&gs->enemies[i]);
        gs->enemies[i].x = ex[i];
        gs->enemies[i].y = ey[i];
    }
}

/* ---------- update ---------- */

static void update_bullets(GameState *gs)
{
    for (int i = 0; i < MAX_BULLETS; i++) {
        Bullet *b = &gs->bullets[i];
        if (!b->active) continue;
        b->x += b->vx; b->y += b->vy;
        if (--b->life <= 0) { b->active = false; continue; }

        if (game_is_wall(gs, b->x, b->y)) {
            spawn_blast(gs, b->x, b->y, 0.7);
            b->active = false;
            continue;
        }
        if (b->from_player) {
            for (int e = 0; e < gs->enemy_count; e++) {
                Tank *en = &gs->enemies[e];
                if (!en->alive) continue;
                if (hypot(b->x - en->x, b->y - en->y) < TANK_R + BULLET_R) {
                    spawn_blast(gs, en->x, en->y, 1.5);
                    en->alive = false; en->respawn = 150;
                    gs->score++;
                    b->active = false;
                    break;
                }
            }
        } else if (gs->player.alive) {
            if (hypot(b->x - gs->player.x, b->y - gs->player.y) < TANK_R + BULLET_R) {
                spawn_blast(gs, gs->player.x, gs->player.y, 1.0);
                gs->player_hp -= 12;
                b->active = false;
                if (gs->player_hp <= 0) {
                    spawn_blast(gs, gs->player.x, gs->player.y, 2.0);
                    gs->player_hp = 100;
                    gs->player.x = PLAYER_START_X;
                    gs->player.y = PLAYER_START_Y;
                }
            }
        }
    }
}

static void update_particles(GameState *gs)
{
    for (int i = 0; i < MAX_PARTICLES; i++) {
        Particle *p = &gs->particles[i];
        if (!p->active) continue;
        p->x += p->vx; p->y += p->vy;
        p->vx *= 0.92; p->vy *= 0.92;
        if (--p->life <= 0) p->active = false;
    }
    for (int i = 0; i < MAX_BLASTS; i++) {
        Blast *bl = &gs->blasts[i];
        if (!bl->active) continue;
        if (++bl->t >= bl->maxt) bl->active = false;
    }
}

void game_update(GameState *gs)
{
    const double SPEED = 2.6;
    const double ROT   = 0.05;
    Tank *p = &gs->player;

    if (gs->key_left)  p->body_angle -= ROT;
    if (gs->key_right) p->body_angle += ROT;

    double move = 0.0;
    if (gs->key_up)   move += SPEED;
    if (gs->key_down) move -= SPEED;
    if (move != 0.0) {
        double nx = p->x + cos(p->body_angle) * move;
        double ny = p->y + sin(p->body_angle) * move;
        if (collision_free(gs, nx, p->y)) p->x = nx;
        if (collision_free(gs, p->x, ny)) p->y = ny;
    }

    p->turret_angle = atan2(gs->mouse_y - p->y, gs->mouse_x - p->x);

    if (gs->player_fire_cd > 0) gs->player_fire_cd--;
    if ((gs->fire_kb || gs->fire_mouse) && gs->player_fire_cd <= 0) {
        spawn_bullet(gs, p, true);
        p->muzzle = 5;
        gs->player_fire_cd = PLAYER_FIRE_CD;
    }
    if (p->muzzle > 0) p->muzzle--;

    for (int i = 0; i < gs->enemy_count; i++) {
        Tank *e = &gs->enemies[i];
        if (e->alive) {
            e->turret_angle = atan2(p->y - e->y, p->x - e->x);
            if (e->muzzle > 0) e->muzzle--;
            if (--e->fire_timer <= 0) {
                spawn_bullet(gs, e, false);
                e->muzzle = 5;
                e->fire_timer = 90 + rand() % 90;
            }
        } else if (--e->respawn <= 0) {
            init_enemy(e);
            place_at_open_tile(gs, e);
        }
    }

    update_bullets(gs);
    update_particles(gs);

    gs->shake *= 0.88;
    gs->ticks++;
}
