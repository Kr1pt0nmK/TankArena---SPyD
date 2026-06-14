#include <gtk/gtk.h>
#include <gdk/gdkkeysyms.h>
#include "render.h"

/* Contexto de la aplicacion: estado del juego + el canvas a redibujar. */
typedef struct {
    GameState gs;
    GtkWidget *canvas;
} App;

static gboolean on_draw(GtkWidget *w, cairo_t *cr, gpointer data)
{
    (void)w;
    App *a = data;
    render_scene(&a->gs, cr);
    return FALSE;
}

static gboolean on_motion(GtkWidget *w, GdkEventMotion *e, gpointer data)
{
    (void)w;
    App *a = data;
    a->gs.mouse_x = e->x;
    a->gs.mouse_y = e->y;
    return FALSE;
}

static void set_key(App *a, guint k, gboolean v)
{
    switch (k) {
        case GDK_KEY_w: case GDK_KEY_W: case GDK_KEY_Up:    a->gs.key_up = v;    break;
        case GDK_KEY_s: case GDK_KEY_S: case GDK_KEY_Down:  a->gs.key_down = v;  break;
        case GDK_KEY_a: case GDK_KEY_A: case GDK_KEY_Left:  a->gs.key_left = v;  break;
        case GDK_KEY_d: case GDK_KEY_D: case GDK_KEY_Right: a->gs.key_right = v; break;
        case GDK_KEY_space:                                 a->gs.fire_kb = v;   break;
        default: break;
    }
}

static gboolean on_button_press(GtkWidget *w, GdkEventButton *e, gpointer data)
{
    (void)w;
    App *a = data;
    a->gs.mouse_x = e->x;
    a->gs.mouse_y = e->y;
    a->gs.fire_mouse = TRUE;
    return FALSE;
}

static gboolean on_button_release(GtkWidget *w, GdkEventButton *e, gpointer data)
{
    (void)w; (void)e;
    App *a = data;
    a->gs.fire_mouse = FALSE;
    return FALSE;
}

static gboolean on_key_press(GtkWidget *w, GdkEventKey *e, gpointer data)
{
    (void)w;
    set_key(data, e->keyval, TRUE);
    return FALSE;
}

static gboolean on_key_release(GtkWidget *w, GdkEventKey *e, gpointer data)
{
    (void)w;
    set_key(data, e->keyval, FALSE);
    return FALSE;
}

/* ~60 FPS: actualiza el estado y pide redibujar. */
static gboolean on_tick(gpointer data)
{
    App *a = data;
    game_update(&a->gs);
    gtk_widget_queue_draw(a->canvas);
    return G_SOURCE_CONTINUE;
}

int main(int argc, char **argv)
{
    gtk_init(&argc, &argv);

    App app;
    game_init(&app.gs);

    GtkBuilder *b = gtk_builder_new_from_file(UI_FILE);
    GtkWidget *win = GTK_WIDGET(gtk_builder_get_object(b, "main_window"));
    app.canvas     = GTK_WIDGET(gtk_builder_get_object(b, "canvas"));

    gtk_widget_add_events(app.canvas, GDK_POINTER_MOTION_MASK |
                                      GDK_BUTTON_PRESS_MASK | GDK_BUTTON_RELEASE_MASK);
    g_signal_connect(app.canvas, "draw",                 G_CALLBACK(on_draw),    &app);
    g_signal_connect(app.canvas, "motion-notify-event",  G_CALLBACK(on_motion),  &app);
    g_signal_connect(app.canvas, "button-press-event",   G_CALLBACK(on_button_press),   &app);
    g_signal_connect(app.canvas, "button-release-event", G_CALLBACK(on_button_release), &app);
    g_signal_connect(win, "key-press-event",   G_CALLBACK(on_key_press),   &app);
    g_signal_connect(win, "key-release-event", G_CALLBACK(on_key_release), &app);
    g_signal_connect(win, "destroy",           G_CALLBACK(gtk_main_quit),  NULL);

    gtk_widget_show_all(win);
    g_timeout_add(16, on_tick, &app);

    gtk_main();
    g_object_unref(b);
    return 0;
}
