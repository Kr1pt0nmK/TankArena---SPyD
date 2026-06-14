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
    cairo_rectangle(cr, -14, -14, VIEW_W + 28, VIEW_H + 28);
    cairo_fill(cr);
    cairo_pattern_destroy(g);

    cairo_set_source_rgba(cr, 1, 1, 1, 0.04);
    cairo_set_line_width(cr, 1.0);
    for (int x = 0; x <= MAP_W; x++) {
        cairo_move_to(cr, x * TILE, 0);
        cairo_line_to(cr, x * TILE, VIEW_H);
    }
    for (int y = 0; y <= MAP_H; y++) {
        cairo_move_to(cr, 0, y * TILE);
        cairo_line_to(cr, VIEW_W, y * TILE);
    }
    cairo_stroke(cr);
}

static void draw_walls(cairo_t *cr, const GameState *gs)
{
    for (int ty = 0; ty < MAP_H; ty++) {
        for (int tx = 0; tx < MAP_W; tx++) {
            if (gs->map[ty * MAP_W + tx] != 1) continue;
            double x = tx * TILE + 2, y = ty * TILE + 2;
            double w = TILE - 4, h = TILE - 4;

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

static void draw_tank(cairo_t *cr, const Tank *t)
{
    if (!t->alive) return;

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
    double hr = t->cr * 1.15, hg = t->cg * 1.15, hb = t->cb * 1.15;
    cairo_pattern_add_color_stop_rgb(body, 0.0, hr > 1 ? 1 : hr, hg > 1 ? 1 : hg, hb > 1 ? 1 : hb);
    cairo_pattern_add_color_stop_rgb(body, 1.0, t->cr * 0.7, t->cg * 0.7, t->cb * 0.7);
    rounded_rect(cr, -15, -11, 30, 22, 5);
    cairo_set_source(cr, body);
    cairo_fill_preserve(cr);
    cairo_pattern_destroy(body);
    cairo_set_source_rgba(cr, 0, 0, 0, 0.35);
    cairo_set_line_width(cr, 1.5);
    cairo_stroke(cr);
    cairo_restore(cr);

    /* torreta + canon + fogonazo */
    cairo_save(cr);
    cairo_translate(cr, t->x, t->y);
    cairo_rotate(cr, t->turret_angle);

    cairo_set_source_rgb(cr, 0.17, 0.18, 0.26);
    rounded_rect(cr, 0, -3.5, 28, 7, 2);
    cairo_fill(cr);

    if (t->muzzle > 0) {
        double f = t->muzzle / 5.0;
        cairo_pattern_t *m = cairo_pattern_create_radial(30, 0, 0, 30, 0, 12 * f + 4);
        cairo_pattern_add_color_stop_rgba(m, 0.0, 1.0, 0.95, 0.6, 0.95);
        cairo_pattern_add_color_stop_rgba(m, 1.0, 1.0, 0.5, 0.1, 0.0);
        cairo_set_source(cr, m);
        cairo_arc(cr, 30, 0, 12 * f + 4, 0, 2 * M_PI);
        cairo_fill(cr);
        cairo_pattern_destroy(m);
    }

    cairo_set_source_rgb(cr, t->cr * 0.75, t->cg * 0.75, t->cb * 0.75);
    cairo_arc(cr, 0, 0, 9, 0, 2 * M_PI);
    cairo_fill_preserve(cr);
    cairo_set_source_rgba(cr, 1, 1, 1, 0.25);
    cairo_set_line_width(cr, 1.5);
    cairo_stroke(cr);
    cairo_restore(cr);
}

static void draw_bullets(cairo_t *cr, const GameState *gs)
{
    for (int i = 0; i < MAX_BULLETS; i++) {
        const Bullet *b = &gs->bullets[i];
        if (!b->active) continue;
        double r = b->from_player ? 0.55 : 1.0;
        double g = b->from_player ? 0.95 : 0.55;
        double bl = b->from_player ? 1.0  : 0.25;

        /* estela */
        cairo_set_source_rgba(cr, r, g, bl, 0.35);
        cairo_set_line_width(cr, 3.0);
        cairo_move_to(cr, b->x - b->vx * 2.5, b->y - b->vy * 2.5);
        cairo_line_to(cr, b->x, b->y);
        cairo_stroke(cr);

        /* glow + nucleo */
        cairo_pattern_t *gl = cairo_pattern_create_radial(b->x, b->y, 0, b->x, b->y, 9);
        cairo_pattern_add_color_stop_rgba(gl, 0.0, r, g, bl, 0.9);
        cairo_pattern_add_color_stop_rgba(gl, 1.0, r, g, bl, 0.0);
        cairo_set_source(cr, gl);
        cairo_arc(cr, b->x, b->y, 9, 0, 2 * M_PI);
        cairo_fill(cr);
        cairo_pattern_destroy(gl);

        cairo_set_source_rgb(cr, 1, 1, 1);
        cairo_arc(cr, b->x, b->y, BULLET_R - 1.5, 0, 2 * M_PI);
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
        cairo_arc(cr, p->x, p->y, p->size, 0, 2 * M_PI);
        cairo_fill(cr);
    }
}

static void draw_blasts(cairo_t *cr, const GameState *gs)
{
    for (int i = 0; i < MAX_BLASTS; i++) {
        const Blast *bl = &gs->blasts[i];
        if (!bl->active) continue;
        double f = bl->t / bl->maxt;          /* 0..1 */
        double rad = (8 + f * 38) * bl->scale;

        cairo_set_source_rgba(cr, 1.0, 0.7, 0.3, (1 - f) * 0.8);
        cairo_set_line_width(cr, 4.0 * (1 - f) + 1);
        cairo_arc(cr, bl->x, bl->y, rad, 0, 2 * M_PI);
        cairo_stroke(cr);

        if (f < 0.4) {
            cairo_pattern_t *fl =
                cairo_pattern_create_radial(bl->x, bl->y, 0, bl->x, bl->y, 18 * bl->scale);
            cairo_pattern_add_color_stop_rgba(fl, 0.0, 1, 1, 0.8, (0.4 - f) * 2.0);
            cairo_pattern_add_color_stop_rgba(fl, 1.0, 1, 0.5, 0.1, 0.0);
            cairo_set_source(cr, fl);
            cairo_arc(cr, bl->x, bl->y, 18 * bl->scale, 0, 2 * M_PI);
            cairo_fill(cr);
            cairo_pattern_destroy(fl);
        }
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
    cairo_rectangle(cr, -14, -14, VIEW_W + 28, VIEW_H + 28);
    cairo_fill(cr);
    cairo_pattern_destroy(v);
}

static void draw_hud(const GameState *gs, cairo_t *cr)
{
    cairo_set_source_rgba(cr, 0, 0, 0, 0.6);
    text_at(cr, 20, 38, "TANK ARENA", 28, CAIRO_FONT_WEIGHT_BOLD);
    cairo_set_source_rgb(cr, 0.96, 0.83, 0.37);
    text_at(cr, 18, 36, "TANK ARENA", 28, CAIRO_FONT_WEIGHT_BOLD);

    cairo_set_source_rgba(cr, 1, 1, 1, 0.7);
    text_at(cr, 19, 56, "avance v0.2  ·  GTK 3 + Cairo", 12, CAIRO_FONT_WEIGHT_NORMAL);

    /* barra de blindaje (arriba a la derecha) */
    double bx = VIEW_W - 196, by = 24, bw = 176, bh = 16;
    double hp = gs->player_hp / 100.0;
    if (hp < 0) hp = 0;
    cairo_set_source_rgba(cr, 1, 1, 1, 0.8);
    text_at(cr, bx, by - 6, "BLINDAJE", 11, CAIRO_FONT_WEIGHT_BOLD);
    cairo_set_source_rgba(cr, 0, 0, 0, 0.5);
    rounded_rect(cr, bx, by, bw, bh, 4); cairo_fill(cr);
    cairo_set_source_rgb(cr, 0.95 - hp * 0.7, 0.25 + hp * 0.55, 0.30);
    rounded_rect(cr, bx + 2, by + 2, (bw - 4) * hp, bh - 4, 3); cairo_fill(cr);

    char buf[64];
    snprintf(buf, sizeof buf, "Bajas: %d", gs->score);
    cairo_set_source_rgb(cr, 0.96, 0.83, 0.37);
    text_at(cr, bx, by + bh + 18, buf, 15, CAIRO_FONT_WEIGHT_BOLD);

    cairo_set_source_rgba(cr, 0, 0, 0, 0.55);
    cairo_rectangle(cr, 0, VIEW_H - 34, VIEW_W, 34);
    cairo_fill(cr);
    cairo_set_source_rgba(cr, 1, 1, 1, 0.85);
    text_at(cr, 16, VIEW_H - 12,
            "WASD / Flechas: mover    ·    Raton: apuntar    ·    Click / Espacio: disparar",
            14, CAIRO_FONT_WEIGHT_NORMAL);
}

void render_scene(const GameState *gs, cairo_t *cr)
{
    /* screen shake (no afecta al HUD) */
    cairo_save(cr);
    if (gs->shake > 0.2) {
        double sx = (((double)rand() / RAND_MAX) - 0.5) * gs->shake * 2.0;
        double sy = (((double)rand() / RAND_MAX) - 0.5) * gs->shake * 2.0;
        cairo_translate(cr, sx, sy);
    }

    draw_background(cr);
    draw_walls(cr, gs);
    for (int i = 0; i < gs->enemy_count; i++)
        draw_tank(cr, &gs->enemies[i]);
    draw_tank(cr, &gs->player);
    draw_bullets(cr, gs);
    draw_particles(cr, gs);
    draw_blasts(cr, gs);
    draw_vignette(cr);

    cairo_restore(cr);
    draw_hud(gs, cr);
}
