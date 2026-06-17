#include <gtk/gtk.h>
#include <gdk/gdkkeysyms.h>
#include <math.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>

#include "render.h"
#include "game.h"
#include "net.h"
#include "thread.h"
#include "protocol.h"
#include "server.h"
#include "client.h"

typedef enum { MODE_SOLO, MODE_HOST, MODE_CLIENT } Mode;

typedef struct {
    GameState gs;
    mutex_t   lock;          /* protege gs en host/cliente */
    GtkWidget *canvas;
    Mode      mode;
    int       local_id;

    double    mouse_x, mouse_y;
    gboolean  kup, kdown, kleft, kright, fkb, fmouse;

    Server   *server;        /* host */
    Client   *client;        /* cliente */
} App;

/* Construye el input local a partir de teclado + raton. */
static Input gather_input(App *a, double px, double py)
{
    Input in;
    memset(&in, 0, sizeof(in));
    in.up = a->kup; in.down = a->kdown; in.left = a->kleft; in.right = a->kright;
    in.fire = a->fkb || a->fmouse;
    in.aim = atan2(a->mouse_y - py, a->mouse_x - px);
    return in;
}

static gboolean on_draw(GtkWidget *w, cairo_t *cr, gpointer data)
{
    (void)w;
    App *a = data;
    if (a->mode == MODE_SOLO) {
        render_scene(&a->gs, cr);
    } else {
        mutex_lock(&a->lock);
        render_scene(&a->gs, cr);
        mutex_unlock(&a->lock);
    }
    return FALSE;
}

static gboolean on_motion(GtkWidget *w, GdkEventMotion *e, gpointer data)
{
    (void)w;
    App *a = data;
    a->mouse_x = e->x; a->mouse_y = e->y;
    return FALSE;
}

static gboolean on_button_press(GtkWidget *w, GdkEventButton *e, gpointer data)
{
    (void)w;
    App *a = data;
    a->mouse_x = e->x; a->mouse_y = e->y; a->fmouse = TRUE;
    return FALSE;
}

static gboolean on_button_release(GtkWidget *w, GdkEventButton *e, gpointer data)
{
    (void)w; (void)e;
    ((App *)data)->fmouse = FALSE;
    return FALSE;
}

static void set_key(App *a, guint k, gboolean v)
{
    switch (k) {
        case GDK_KEY_w: case GDK_KEY_W: case GDK_KEY_Up:    a->kup = v;    break;
        case GDK_KEY_s: case GDK_KEY_S: case GDK_KEY_Down:  a->kdown = v;  break;
        case GDK_KEY_a: case GDK_KEY_A: case GDK_KEY_Left:  a->kleft = v;  break;
        case GDK_KEY_d: case GDK_KEY_D: case GDK_KEY_Right: a->kright = v; break;
        case GDK_KEY_space:                                 a->fkb = v;    break;
        default: break;
    }
}

static gboolean on_key_press(GtkWidget *w, GdkEventKey *e, gpointer data)
{ (void)w; set_key(data, e->keyval, TRUE);  return FALSE; }
static gboolean on_key_release(GtkWidget *w, GdkEventKey *e, gpointer data)
{ (void)w; set_key(data, e->keyval, FALSE); return FALSE; }

static gboolean on_tick(gpointer data)
{
    App *a = data;

    if (a->mode == MODE_SOLO) {
        Tank *p = &a->gs.players[a->local_id];
        p->in = gather_input(a, p->x, p->y);
        game_update(&a->gs);
    } else if (a->mode == MODE_HOST) {
        /* la simulacion la corre el hilo del servidor; aqui solo capturo mi input */
        mutex_lock(&a->lock);
        Tank *p = &a->gs.players[a->local_id];
        double px = p->x, py = p->y;
        Input in = gather_input(a, px, py);
        a->gs.players[a->local_id].in = in;
        mutex_unlock(&a->lock);
    } else { /* CLIENT */
        double px = VIEW_W * 0.5, py = VIEW_H * 0.5;
        if (a->local_id >= 0) {
            mutex_lock(&a->lock);
            px = a->gs.players[a->local_id].x;
            py = a->gs.players[a->local_id].y;
            mutex_unlock(&a->lock);
        }
        Input in = gather_input(a, px, py);
        client_send_input(a->client, &in);
    }

    gtk_widget_queue_draw(a->canvas);
    return G_SOURCE_CONTINUE;
}

static void usage(const char *prog)
{
    fprintf(stderr,
        "Uso:\n"
        "  %s                       juego local (1 jugador vs IA)\n"
        "  %s host [puerto]         hospeda una partida\n"
        "  %s client <ip> [puerto]  se conecta a un host\n",
        prog, prog, prog);
}

int main(int argc, char **argv)
{
    gtk_init(&argc, &argv);

    App app;
    memset(&app, 0, sizeof(app));
    mutex_init(&app.lock);

    /* --- parseo de argumentos --- */
    app.mode = MODE_SOLO;
    const char *host_ip = "127.0.0.1";
    uint16_t port = PROTO_PORT;

    if (argc >= 2) {
        if (strcmp(argv[1], "host") == 0) {
            app.mode = MODE_HOST;
            if (argc >= 3) port = (uint16_t)atoi(argv[2]);
        } else if (strcmp(argv[1], "client") == 0) {
            app.mode = MODE_CLIENT;
            if (argc < 3) { usage(argv[0]); return 1; }
            host_ip = argv[2];
            if (argc >= 4) port = (uint16_t)atoi(argv[3]);
        } else if (strcmp(argv[1], "solo") != 0) {
            usage(argv[0]);
            return 1;
        }
    }

    game_init(&app.gs);

    if (app.mode != MODE_SOLO && net_init() != 0) {
        fprintf(stderr, "Error: no se pudo iniciar la red.\n");
        return 1;
    }

    if (app.mode == MODE_SOLO) {
        app.local_id = 0;
        game_add_player(&app.gs, 0);
        app.gs.local_id = 0;
    } else if (app.mode == MODE_HOST) {
        app.local_id = 0;
        game_add_player(&app.gs, 0);
        app.gs.local_id = 0;
        app.server = server_start(&app.gs, &app.lock, port);
        if (!app.server) { fprintf(stderr, "Error: no se pudo hospedar en el puerto %u.\n", port); return 1; }
    } else { /* CLIENT */
        app.client = client_connect(host_ip, port, &app.gs, &app.lock, &app.local_id);
        if (!app.client) { fprintf(stderr, "Error: no se pudo conectar a %s:%u.\n", host_ip, port); return 1; }
        app.gs.local_id = app.local_id;
    }

    GtkBuilder *b = gtk_builder_new_from_file(UI_FILE);
    GtkWidget *win = GTK_WIDGET(gtk_builder_get_object(b, "main_window"));
    app.canvas     = GTK_WIDGET(gtk_builder_get_object(b, "canvas"));

    const char *title = app.mode == MODE_HOST   ? "Tank Arena — HOST"
                      : app.mode == MODE_CLIENT ? "Tank Arena — CLIENTE"
                                                : "Tank Arena — Local";
    gtk_window_set_title(GTK_WINDOW(win), title);

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

    /* --- limpieza --- */
    if (app.server) server_stop(app.server);
    if (app.client) client_close(app.client);
    if (app.mode != MODE_SOLO) net_cleanup();
    g_object_unref(b);
    mutex_destroy(&app.lock);
    return 0;
}
