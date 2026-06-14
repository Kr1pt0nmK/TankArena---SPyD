#ifndef RENDER_H
#define RENDER_H

#include <cairo.h>
#include "game.h"

/* Dibuja toda la escena del juego sobre el contexto Cairo dado. */
void render_scene(const GameState *gs, cairo_t *cr);

#endif /* RENDER_H */
