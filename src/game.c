#include "game.h"
#include <math.h>
#include <stdlib.h>
#include <time.h>

#define PLAYER_FIRE_CD 12
#define BULLET_SPEED   6.0
#define BULLET_LIFE    170
#define MOVE_SPEED     2.6
#define ROT_SPEED      0.05
#define FOOT_SPEED     1.7    /* el soldado a pie se mueve mas lento */
#define FOOT_R         8.0    /* radio de colision del soldado */
#define ROUND_RESTART  180    /* ticks que dura el anuncio antes de reiniciar (~3 s) */

/* ---- Puntos de aparicion de los jugadores (zona baja, separados) ---- */
static const double SPAWN_X[MAX_PLAYERS] = {
    VIEW_W * 0.5, TILE * 2.5, VIEW_W - TILE * 2.5, VIEW_W * 0.5
};
static const double SPAWN_Y[MAX_PLAYERS] = {
    VIEW_H - TILE * 2.5, VIEW_H - TILE * 2.5, VIEW_H - TILE * 2.5, VIEW_H - TILE * 4.0
};

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

/* ---------- visuales ---------- */

void game_set_player_visual(Tank *t, int id)
{
    static const double C[MAX_PLAYERS][3] = {
        { 0.18, 0.77, 0.71 },  /* cian   */
        { 0.96, 0.78, 0.25 },  /* amarillo */
        { 0.66, 0.45, 0.95 },  /* morado */
        { 0.40, 0.85, 0.40 },  /* verde  */
    };
    int k = id & 3;
    t->cr = C[k][0]; t->cg = C[k][1]; t->cb = C[k][2];
}

void game_set_enemy_visual(Tank *t) { t->cr = 0.90; t->cg = 0.22; t->cb = 0.27; }

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
    int n = (int)(12 * scale);
    for (int i = 0; i < n; i++) {
        double a = frand() * M_PI * 2.0;
        double sp = (1.5 + frand() * 4.0) * scale;
        double tint = 0.6 + frand() * 0.4;
        spawn_particle(gs, x, y, cos(a) * sp, sin(a) * sp,
                       12 + frand() * 16, 2 + frand() * 3,
                       1.0, tint, 0.2 + frand() * 0.2);
    }
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

/* Estallido de sangre: cuando matan a un soldado a pie. Particulas rojas que
   salen disparadas, mas unas gotas oscuras grandes. */
static void spawn_blood(GameState *gs, double x, double y)
{
    for (int i = 0; i < 24; i++) {
        double a  = frand() * M_PI * 2.0;
        double sp = 1.0 + frand() * 5.0;
        double r  = 0.6 + frand() * 0.35;
        spawn_particle(gs, x, y, cos(a) * sp, sin(a) * sp,
                       16 + frand() * 22, 2 + frand() * 3.5,
                       r, frand() * 0.08, frand() * 0.06);
    }
    for (int i = 0; i < 6; i++) {   /* gotas grandes y oscuras */
        double a  = frand() * M_PI * 2.0;
        double sp = 0.4 + frand() * 2.0;
        spawn_particle(gs, x, y, cos(a) * sp, sin(a) * sp,
                       30 + frand() * 22, 4 + frand() * 4, 0.45, 0.0, 0.0);
    }
    gs->shake += 3.5;
    if (gs->shake > 12) gs->shake = 12;
}

static void spawn_bullet(GameState *gs, const Tank *t, int owner)
{
    double tipx = t->x + cos(t->turret_angle) * 30.0;
    double tipy = t->y + sin(t->turret_angle) * 30.0;
    for (int i = 0; i < MAX_BULLETS; i++) {
        Bullet *b = &gs->bullets[i];
        if (b->active) continue;
        b->x = tipx; b->y = tipy;
        b->vx = cos(t->turret_angle) * BULLET_SPEED;
        b->vy = sin(t->turret_angle) * BULLET_SPEED;
        b->life = BULLET_LIFE; b->active = true; b->owner = owner;
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

        bool near = false;
        for (int i = 0; i < MAX_PLAYERS; i++) {
            Tank *p = &gs->players[i];
            if (p->active && hypot(cx - p->x, cy - p->y) < 4 * TILE) { near = true; break; }
        }
        if (near) continue;
        t->x = cx; t->y = cy;
        return;
    }
}

static void init_enemy(Tank *e)
{
    e->body_angle = e->turret_angle = M_PI / 2.0;
    game_set_enemy_visual(e);
    e->alive = true; e->active = true; e->muzzle = 0; e->hp = 1;
    e->fire_timer = 60 + rand() % 120;
    e->respawn = 0;
}

/* ---------- init / jugadores ---------- */

void game_add_player(GameState *gs, int id)
{
    if (id < 0 || id >= MAX_PLAYERS) return;
    Tank *p = &gs->players[id];
    p->x = SPAWN_X[id]; p->y = SPAWN_Y[id];
    p->body_angle = -M_PI / 2.0;
    p->turret_angle = -M_PI / 2.0;
    game_set_player_visual(p, id);
    p->active = true; p->alive = true;
    p->on_foot = false; p->eliminated = false;
    p->hp = 100; p->score = 0;
    p->muzzle = 0; p->fire_cd = 0; p->respawn = 0;
    p->in = (Input){0};
}

void game_remove_player(GameState *gs, int id)
{
    if (id < 0 || id >= MAX_PLAYERS) return;
    gs->players[id].active = false;
    gs->players[id].alive = false;
}

void game_init(GameState *gs)
{
    srand((unsigned)time(NULL));
    build_map();
    gs->map = g_map;
    gs->ticks = 0;
    gs->shake = 0;
    gs->local_id = -1;
    gs->round_over = 0;
    gs->round_winner = -1;
    gs->round_timer = 0;

    for (int i = 0; i < MAX_PLAYERS; i++) {
        gs->players[i].active = false; gs->players[i].alive = false;
        gs->players[i].on_foot = false; gs->players[i].eliminated = false;
    }
    for (int i = 0; i < MAX_BULLETS; i++)   gs->bullets[i].active = false;
    for (int i = 0; i < MAX_PARTICLES; i++) gs->particles[i].active = false;
    for (int i = 0; i < MAX_BLASTS; i++)    gs->blasts[i].active = false;

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

static Tank *nearest_player(GameState *gs, const Tank *from)
{
    Tank *best = NULL;
    double bd = 1e18;
    for (int i = 0; i < MAX_PLAYERS; i++) {
        Tank *p = &gs->players[i];
        if (!p->active || !p->alive) continue;
        double d = hypot(p->x - from->x, p->y - from->y);
        if (d < bd) { bd = d; best = p; }
    }
    return best;
}

static void update_players(GameState *gs)
{
    for (int i = 0; i < MAX_PLAYERS; i++) {
        Tank *p = &gs->players[i];
        if (!p->active) continue;

        if (p->alive) {
            Input *in = &p->in;
            if (in->left)  p->body_angle -= ROT_SPEED;
            if (in->right) p->body_angle += ROT_SPEED;

            double mv = 0.0;
            if (in->up)   mv += MOVE_SPEED;
            if (in->down) mv -= MOVE_SPEED;
            if (mv != 0.0) {
                double nx = p->x + cos(p->body_angle) * mv;
                double ny = p->y + sin(p->body_angle) * mv;
                if (collision_free(gs, nx, p->y)) p->x = nx;
                if (collision_free(gs, p->x, ny)) p->y = ny;
            }
            p->turret_angle = in->aim;

            if (p->fire_cd > 0) p->fire_cd--;
            if (in->fire && p->fire_cd == 0) {
                spawn_bullet(gs, p, i);
                p->fire_cd = PLAYER_FIRE_CD;
                p->muzzle = 4;
            }
            if (p->muzzle > 0) p->muzzle--;
        } else if (p->on_foot) {
            /* soldado a pie: se mueve mas lento, no dispara, y busca robar un
               tanque enemigo para revivir. */
            Input *in = &p->in;
            if (in->left)  p->body_angle -= ROT_SPEED;
            if (in->right) p->body_angle += ROT_SPEED;

            double mv = 0.0;
            if (in->up)   mv += FOOT_SPEED;
            if (in->down) mv -= FOOT_SPEED;
            if (mv != 0.0) {
                double nx = p->x + cos(p->body_angle) * mv;
                double ny = p->y + sin(p->body_angle) * mv;
                if (!game_is_wall(gs, nx, p->y)) p->x = nx;
                if (!game_is_wall(gs, p->x, ny)) p->y = ny;
            }
            p->turret_angle = in->aim;

            /* robar: al tocar un tanque enemigo vivo, revive en su lugar */
            for (int e = 0; e < gs->enemy_count; e++) {
                Tank *en = &gs->enemies[e];
                if (!en->alive) continue;
                if (hypot(p->x - en->x, p->y - en->y) < TANK_R + FOOT_R) {
                    p->on_foot = false;
                    p->alive = true;
                    p->hp = 100;
                    p->x = en->x; p->y = en->y;
                    p->body_angle = en->body_angle;
                    en->alive = false; en->respawn = 150;
                    spawn_blast(gs, p->x, p->y, 1.0);
                    break;
                }
            }
        }
        /* eliminados: no hacen nada (esperan a que termine la ronda) */
    }
}

static void update_enemies(GameState *gs)
{
    for (int e = 0; e < gs->enemy_count; e++) {
        Tank *en = &gs->enemies[e];
        if (en->alive) {
            Tank *tg = nearest_player(gs, en);
            if (tg) en->turret_angle = atan2(tg->y - en->y, tg->x - en->x);
            if (--en->fire_timer <= 0) {
                if (tg) { spawn_bullet(gs, en, -1); en->muzzle = 4; }
                en->fire_timer = 70 + rand() % 120;
            }
            if (en->muzzle > 0) en->muzzle--;
        } else {
            if (en->respawn > 0 && --en->respawn == 0) {
                init_enemy(en);
                place_at_open_tile(gs, en);
            }
        }
    }
}

/* Una bala (disparada por 'shooter': id de jugador, o <0 si es enemigo) golpea
   a un jugador. Al tanque le baja blindaje (y si llega a 0 baja a pie); al
   soldado a pie lo elimina de la ronda. Devuelve true si impacto a alguien. */
static bool bullet_hits_player(GameState *gs, double bx, double by, int shooter)
{
    for (int i = 0; i < MAX_PLAYERS; i++) {
        if (i == shooter) continue;            /* no te dañas a ti mismo */
        Tank *pl = &gs->players[i];
        if (!pl->active) continue;

        if (pl->alive) {
            if (hypot(bx - pl->x, by - pl->y) < TANK_R + BULLET_R) {
                spawn_blast(gs, pl->x, pl->y, 1.0);
                pl->hp -= 12;
                if (pl->hp <= 0) {
                    spawn_blast(gs, pl->x, pl->y, 2.0);
                    pl->alive = false;
                    pl->on_foot = true;        /* el tanque revienta -> sale a pie */
                    pl->hp = 0;
                    if (shooter >= 0 && shooter < MAX_PLAYERS && gs->players[shooter].active)
                        gs->players[shooter].score++;
                }
                return true;
            }
        } else if (pl->on_foot) {
            if (hypot(bx - pl->x, by - pl->y) < FOOT_R + BULLET_R) {
                spawn_blood(gs, pl->x, pl->y);
                pl->on_foot = false;
                pl->eliminated = true;         /* mataron al soldado -> fuera de la ronda */
                if (shooter >= 0 && shooter < MAX_PLAYERS && gs->players[shooter].active)
                    gs->players[shooter].score++;
                return true;
            }
        }
    }
    return false;
}

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

        if (b->owner >= 0) {
            /* bala de jugador: pega a enemigos */
            for (int e = 0; e < gs->enemy_count; e++) {
                Tank *en = &gs->enemies[e];
                if (!en->alive) continue;
                if (hypot(b->x - en->x, b->y - en->y) < TANK_R + BULLET_R) {
                    spawn_blast(gs, en->x, en->y, 1.5);
                    en->alive = false; en->respawn = 150;
                    if (b->owner < MAX_PLAYERS && gs->players[b->owner].active)
                        gs->players[b->owner].score++;
                    b->active = false;
                    break;
                }
            }
            /* PvP + soldados: tambien pega a otros jugadores (excluye al tirador). */
            if (b->active && bullet_hits_player(gs, b->x, b->y, b->owner))
                b->active = false;
        } else {
            /* bala de enemigo: pega a cualquier jugador o soldado */
            if (bullet_hits_player(gs, b->x, b->y, -1))
                b->active = false;
        }
    }
}

static void update_particles(GameState *gs)
{
    for (int i = 0; i < MAX_PARTICLES; i++) {
        Particle *p = &gs->particles[i];
        if (!p->active) continue;
        p->x += p->vx; p->y += p->vy;
        p->vx *= 0.94; p->vy *= 0.94;
        if (--p->life <= 0) p->active = false;
    }
    for (int i = 0; i < MAX_BLASTS; i++) {
        Blast *bl = &gs->blasts[i];
        if (!bl->active) continue;
        if (++bl->t >= bl->maxt) bl->active = false;
    }
}

/* Revive a todos los jugadores activos como tanques con vida llena (nueva ronda). */
static void respawn_all_players(GameState *gs)
{
    for (int i = 0; i < MAX_PLAYERS; i++) {
        Tank *p = &gs->players[i];
        if (!p->active) continue;
        p->alive = true; p->on_foot = false; p->eliminated = false;
        p->hp = 100; p->fire_cd = 0; p->muzzle = 0;
        p->x = SPAWN_X[i]; p->y = SPAWN_Y[i];
        p->body_angle = p->turret_angle = -M_PI / 2.0;
    }
}

/* Controla la ronda: termina cuando queda un solo jugador en pie (los demas
   eliminados), anuncia al ganador y reinicia tras unos segundos. */
static void update_round(GameState *gs)
{
    if (gs->round_over) {
        if (--gs->round_timer <= 0) {
            respawn_all_players(gs);
            gs->round_over = 0;
            gs->round_winner = -1;
        }
        return;
    }

    int active = 0, inplay = 0, last = -1;
    for (int i = 0; i < MAX_PLAYERS; i++) {
        if (!gs->players[i].active) continue;
        active++;
        if (!gs->players[i].eliminated) { inplay++; last = i; }
    }

    if (active >= 2 && inplay <= 1) {
        /* fin de ronda: queda un solo superviviente */
        gs->round_over = 1;
        gs->round_winner = (inplay == 1) ? last : -1;
        gs->round_timer = ROUND_RESTART;
    } else if (active == 1 && inplay == 0) {
        /* en solitario: si el unico jugador cae a pie y muere, reinicia */
        respawn_all_players(gs);
    }
}

void game_update(GameState *gs)
{
    update_players(gs);
    update_enemies(gs);
    update_bullets(gs);
    update_round(gs);
    update_particles(gs);
    if (gs->shake > 0) { gs->shake -= 0.6; if (gs->shake < 0) gs->shake = 0; }
    gs->ticks++;
}
