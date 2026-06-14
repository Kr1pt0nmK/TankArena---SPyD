/* Genera una imagen PNG de un frame del juego, sin abrir ventana.
   Sirve para ver el render sin entorno grafico (CI, capturas, etc.). */
#include <cairo.h>
#include "render.h"

int main(void)
{
    GameState gs;
    game_init(&gs);
    game_add_player(&gs, 0);
    gs.local_id = 0;

    /* Apunta la torreta y dispara para capturar balas, fogonazo y explosiones. */
    Tank *p = &gs.players[0];
    p->in.aim = -0.6;
    p->in.fire = true;
    for (int i = 0; i < 70; i++)
        game_update(&gs);

    cairo_surface_t *s =
        cairo_image_surface_create(CAIRO_FORMAT_ARGB32, VIEW_W, VIEW_H);
    cairo_t *cr = cairo_create(s);
    render_scene(&gs, cr);
    cairo_surface_write_to_png(s, "preview.png");
    cairo_destroy(cr);
    cairo_surface_destroy(s);
    return 0;
}
