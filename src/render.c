#include "render.h"
#include <math.h>
#include <stdlib.h>
#include <stdio.h>

/* ---------- helpers ---------- */

static void rounded_rect(cairo_t *cr, double x, double y,
                         double w, double h, double r)
{
    cairo_new_sub_path(cr);
    cairo_arc(cr, x + w - r, y + r,     r, -M_PI / 2, 0);
    cairo_arc(cr, x + w - r, y + h - r, r, 0,          M_PI / 2);
    cairo_arc(cr, x + r,     y + h - r, r, M_PI / 2,   M_PI);
    cairo_arc(cr, x + r,     y + r,     r, M_PI,       M_PI * 1.5);
    cairo_close_path(cr);
}

static void text_at(cairo_t *cr, double x, double y, const char *s,
                    double size, cairo_font_weight_t weight)
{
    cairo_select_font_face(cr, "Sans", CAIRO_FONT_SLANT_NORMAL, weight);
    cairo_set_font_size(cr, size);
    cairo_move_to(cr, x, y);
    cairo_show_text(cr, s);
}

/* ---------- capas ---------- */

static void draw_background(cairo_t *cr)
{
    cairo_pattern_t *g = cairo_pattern_create_linear(0, 0, 0, VIEW_H);
    cairo_pattern_add_color_stop_rgb(g, 0.0, 0.05, 0.10, 0.16);
    cairo_pattern_add_color_stop_rgb(g, 1.0, 0.10, 0.15, 0.23);
    cairo_set_source(cr, g);
    cairo_paint(cr);
    cairo_pattern_destroy(g);

    cairo_set_source_rgba(cr, 1, 1, 1, 0.04);
    cairo_set_line_width(cr, 1.0);
    for (int x = 0; x <= MAP_W; x++) { cairo_move_to(cr, x * TILE, 0); cairo_line_to(cr, x * TILE, VIEW_H); }
    for (int y = 0; y <= MAP_H; y++) { cairo_move_to(cr, 0, y * TILE); cairo_line_to(cr, VIEW_W, y * TILE); }
    cairo_stroke(cr);
}

static void draw_walls(cairo_t *cr, const GameState *gs)
{
    for (int ty = 0; ty < MAP_H; ty++) {
        for (int tx = 0; tx < MAP_W; tx++) {
            if (gs->map[ty * MAP_W + tx] != 1) continue;
            double x = tx * TILE + 2, y = ty * TILE + 2, w = TILE - 4, h = TILE - 4;

            cairo_pattern_t *g = cairo_pattern_create_linear(x, y, x, y + h);
            cairo_pattern_add_color_stop_rgb(g, 0.0, 0.88, 0.48, 0.25);
            cairo_pattern_add_color_stop_rgb(g, 1.0, 0.70, 0.34, 0.16);
            rounded_rect(cr, x, y, w, h, 5);
            cairo_set_source(cr, g);
            cairo_fill_preserve(cr);
            cairo_pattern_destroy(g);

            cairo_set_source_rgb(cr, 0.52, 0.25, 0.11);
            cairo_set_line_width(cr, 1.5);
            cairo_stroke(cr);

            cairo_set_source_rgba(cr, 1, 1, 1, 0.18);
            cairo_set_line_width(cr, 2.0);
            cairo_move_to(cr, x + 5, y + 3);
            cairo_line_to(cr, x + w - 5, y + 3);
            cairo_stroke(cr);
        }
    }
}

static void draw_tank(cairo_t *cr, const Tank *t, bool is_local)
{
    if (!t->active || !t->alive) return;

    cairo_pattern_t *glow =
        cairo_pattern_create_radial(t->x, t->y, 2, t->x, t->y, 30);
    cairo_pattern_add_color_stop_rgba(glow, 0.0, t->cr, t->cg, t->cb, 0.35);
    cairo_pattern_add_color_stop_rgba(glow, 1.0, t->cr, t->cg, t->cb, 0.0);
    cairo_set_source(cr, glow);
    cairo_arc(cr, t->x, t->y, 30, 0, 2 * M_PI);
    cairo_fill(cr);
    cairo_pattern_destroy(glow);

    /* chasis + orugas */
    cairo_save(cr);
    cairo_translate(cr, t->x, t->y);
    cairo_rotate(cr, t->body_angle);
    cairo_set_source_rgb(cr, t->cr * 0.45, t->cg * 0.45, t->cb * 0.45);
    rounded_rect(cr, -16, -17, 32, 8, 2); cairo_fill(cr);
    rounded_rect(cr, -16,   9, 32, 8, 2); cairo_fill(cr);

    cairo_pattern_t *body = cairo_pattern_create_linear(0, -11, 0, 11);
    double hr = t->cr * 1.15 > 1 ? 1 : t->cr * 1.15;
    double hg = t->cg * 1.15 > 1 ? 1 : t->cg * 1.15;
    double hb = t->cb * 1.15 > 1 ? 1 : t->cb * 1.15;
    cairo_pattern_add_color_stop_rgb(body, 0.0, hr, hg, hb);
    cairo_pattern_add_color_stop_rgb(body, 1.0, t->cr * 0.7, t->cg * 0.7, t->cb * 0.7);
    rounded_rect(cr, -15, -11, 30, 22, 5);
    cairo_set_source(cr, body);
    cairo_fill_preserve(cr);
    cairo_pattern_destroy(body);
    cairo_set_source_rgba(cr, 0, 0, 0, 0.35);
    cairo_set_line_width(cr, 1.5);
    cairo_stroke(cr);
    cairo_restore(cr);

    /* torreta + canon */
    cairo_save(cr);
    cairo_translate(cr, t->x, t->y);
    cairo_rotate(cr, t->turret_angle);
    cairo_set_source_rgb(cr, 0.17, 0.18, 0.26);
    rounded_rect(cr, 0, -3.5, 28, 7, 2);
    cairo_fill(cr);
    cairo_set_source_rgb(cr, t->cr * 0.75, t->cg * 0.75, t->cb * 0.75);
    cairo_arc(cr, 0, 0, 9, 0, 2 * M_PI);
    cairo_fill_preserve(cr);
    cairo_set_source_rgba(cr, 1, 1, 1, 0.25);
    cairo_set_line_width(cr, 1.5);
    cairo_stroke(cr);

    if (t->muzzle > 0) {
        cairo_set_source_rgba(cr, 1.0, 0.9, 0.4, 0.9);
        cairo_arc(cr, 30, 0, 5, 0, 2 * M_PI);
        cairo_fill(cr);
    }
    cairo_restore(cr);

    /* anillo para el jugador local */
    if (is_local) {
        cairo_set_source_rgba(cr, 1, 1, 1, 0.8);
        cairo_set_line_width(cr, 2.0);
        cairo_arc(cr, t->x, t->y, 21, 0, 2 * M_PI);
        cairo_stroke(cr);
    }
}

static void draw_bullets(cairo_t *cr, const GameState *gs)
{
    for (int i = 0; i < MAX_BULLETS; i++) {
        const Bullet *b = &gs->bullets[i];
        if (!b->active) continue;
        double r, g, bl;
        if (b->owner >= 0) { r = 0.4; g = 0.9; bl = 1.0; }   /* jugador: cian */
        else               { r = 1.0; g = 0.6; bl = 0.2; }   /* enemigo: naranja */

        cairo_pattern_t *gl = cairo_pattern_create_radial(b->x, b->y, 0, b->x, b->y, 8);
        cairo_pattern_add_color_stop_rgba(gl, 0.0, r, g, bl, 0.9);
        cairo_pattern_add_color_stop_rgba(gl, 1.0, r, g, bl, 0.0);
        cairo_set_source(cr, gl);
        cairo_arc(cr, b->x, b->y, 8, 0, 2 * M_PI);
        cairo_fill(cr);
        cairo_pattern_destroy(gl);

        cairo_set_source_rgb(cr, 1, 1, 1);
        cairo_arc(cr, b->x, b->y, 2.5, 0, 2 * M_PI);
        cairo_fill(cr);
    }
}

static void draw_particles(cairo_t *cr, const GameState *gs)
{
    for (int i = 0; i < MAX_PARTICLES; i++) {
        const Particle *p = &gs->particles[i];
        if (!p->active) continue;
        double a = p->life / p->maxlife;
        cairo_set_source_rgba(cr, p->r, p->g, p->b, a);
        cairo_arc(cr, p->x, p->y, p->size * a, 0, 2 * M_PI);
        cairo_fill(cr);
    }
}

static void draw_blasts(cairo_t *cr, const GameState *gs)
{
    for (int i = 0; i < MAX_BLASTS; i++) {
        const Blast *bl = &gs->blasts[i];
        if (!bl->active) continue;
        double a = bl->t / bl->maxt;
        double rad = (8 + a * 34) * bl->scale;
        cairo_set_source_rgba(cr, 1.0, 0.7 - a * 0.5, 0.2, (1.0 - a) * 0.8);
        cairo_set_line_width(cr, 3.0 * (1.0 - a) + 0.5);
        cairo_arc(cr, bl->x, bl->y, rad, 0, 2 * M_PI);
        cairo_stroke(cr);
    }
}

static void draw_vignette(cairo_t *cr)
{
    cairo_pattern_t *v = cairo_pattern_create_radial(
        VIEW_W / 2.0, VIEW_H / 2.0, VIEW_H * 0.3,
        VIEW_W / 2.0, VIEW_H / 2.0, VIEW_H * 0.75);
    cairo_pattern_add_color_stop_rgba(v, 0.0, 0, 0, 0, 0.0);
    cairo_pattern_add_color_stop_rgba(v, 1.0, 0, 0, 0, 0.40);
    cairo_set_source(cr, v);
    cairo_paint(cr);
    cairo_pattern_destroy(v);
}

static void draw_hud(cairo_t *cr, const GameState *gs)
{
    cairo_set_source_rgba(cr, 0, 0, 0, 0.6);
    text_at(cr, 20, 38, "TANK ARENA", 28, CAIRO_FONT_WEIGHT_BOLD);
    cairo_set_source_rgb(cr, 0.96, 0.83, 0.37);
    text_at(cr, 18, 36, "TANK ARENA", 28, CAIRO_FONT_WEIGHT_BOLD);

    /* datos del jugador local */
    const Tank *me = NULL;
    if (gs->local_id >= 0 && gs->local_id < MAX_PLAYERS && gs->players[gs->local_id].active)
        me = &gs->players[gs->local_id];

    if (me) {
        double hp = me->hp < 0 ? 0 : me->hp;
        cairo_set_source_rgb(cr, 0.96, 0.83, 0.37);
        text_at(cr, VIEW_W - 165, 28, "BLINDAJE", 13, CAIRO_FONT_WEIGHT_BOLD);
        cairo_set_source_rgba(cr, 0, 0, 0, 0.5);
        rounded_rect(cr, VIEW_W - 165, 34, 145, 14, 3); cairo_fill(cr);
        double frac = hp / 100.0;
        cairo_set_source_rgb(cr, 1.0 - frac * 0.7, 0.3 + frac * 0.5, 0.3);
        rounded_rect(cr, VIEW_W - 165, 34, 145 * frac, 14, 3); cairo_fill(cr);

        char buf[64];
        snprintf(buf, sizeof(buf), "Bajas: %d", me->score);
        cairo_set_source_rgb(cr, 0.96, 0.83, 0.37);
        text_at(cr, VIEW_W - 165, 66, buf, 16, CAIRO_FONT_WEIGHT_BOLD);
    }

    /* numero de jugadores conectados */
    int np = 0;
    for (int i = 0; i < MAX_PLAYERS; i++) if (gs->players[i].active) np++;
    char pbuf[48];
    snprintf(pbuf, sizeof(pbuf), "Jugadores: %d", np);
    cairo_set_source_rgba(cr, 1, 1, 1, 0.8);
    text_at(cr, 19, 56, pbuf, 13, CAIRO_FONT_WEIGHT_NORMAL);

    cairo_set_source_rgba(cr, 0, 0, 0, 0.55);
    cairo_rectangle(cr, 0, VIEW_H - 34, VIEW_W, 34);
    cairo_fill(cr);
    cairo_set_source_rgba(cr, 1, 1, 1, 0.85);
    text_at(cr, 16, VIEW_H - 12,
            "WASD / Flechas: mover      ·      Raton: apuntar      ·      Click / Espacio: disparar",
            14, CAIRO_FONT_WEIGHT_NORMAL);
}

void render_scene(const GameState *gs, cairo_t *cr)
{
    /* screen shake */
    double ox = 0, oy = 0;
    if (gs->shake > 0) {
        ox = (rand() / (double)RAND_MAX - 0.5) * gs->shake;
        oy = (rand() / (double)RAND_MAX - 0.5) * gs->shake;
    }
    cairo_save(cr);
    cairo_translate(cr, ox, oy);

    draw_background(cr);
    draw_walls(cr, gs);
    for (int e = 0; e < gs->enemy_count; e++) draw_tank(cr, &gs->enemies[e], false);
    for (int i = 0; i < MAX_PLAYERS; i++) draw_tank(cr, &gs->players[i], i == gs->local_id);
    draw_bullets(cr, gs);
    draw_particles(cr, gs);
    draw_blasts(cr, gs);
    draw_vignette(cr);
    cairo_restore(cr);

    draw_hud(cr, gs);
}
