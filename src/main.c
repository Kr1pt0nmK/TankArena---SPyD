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
    GtkWidget *chat_view;    /* historial de chat (solo lectura) */
    GtkWidget *chat_entry;   /* caja para escribir */
    Mode      mode;
    int       local_id;

    double    mouse_x, mouse_y;
    gboolean  kup, kdown, kleft, kright, fkb, fmouse;

    Server   *server;        /* host */
    Client   *client;        /* cliente */
    uint16_t  port;          /* puerto (para reconectar/migrar) */
    gboolean  lost;          /* true si se perdio la partida sin poder migrar */
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
    gtk_widget_grab_focus(a->canvas);   /* clic en el juego = recupera el control del tanque */
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

/* Suelta todas las teclas (evita que el tanque se quede girando al cambiar de
   foco o de ventana con una tecla apretada). */
static void clear_keys(App *a)
{
    a->kup = a->kdown = a->kleft = a->kright = a->fkb = a->fmouse = FALSE;
}

static gboolean on_key_press(GtkWidget *w, GdkEventKey *e, gpointer data)
{
    (void)w;
    App *a = data;
    gboolean typing = gtk_widget_has_focus(a->chat_entry);

    /* Enter (fuera del chat): salta a la caja de texto para escribir. */
    if ((e->keyval == GDK_KEY_Return || e->keyval == GDK_KEY_KP_Enter) && !typing) {
        clear_keys(a);                      /* suelta todo al entrar al chat */
        gtk_widget_grab_focus(a->chat_entry);
        return TRUE;
    }
    /* Escape (dentro del chat): vuelve al juego sin enviar. */
    if (e->keyval == GDK_KEY_Escape && typing) {
        gtk_widget_grab_focus(a->canvas);
        return TRUE;
    }
    /* Tab no debe pasar el foco al chat mientras juegas (si no, WASD se escribe). */
    if (!typing && (e->keyval == GDK_KEY_Tab || e->keyval == GDK_KEY_ISO_Left_Tab))
        return TRUE;
    /* Mientras escribes, las teclas van a la caja, no al tanque. */
    if (typing) return FALSE;

    set_key(a, e->keyval, TRUE);
    return FALSE;
}
static gboolean on_key_release(GtkWidget *w, GdkEventKey *e, gpointer data)
{
    (void)w;
    /* Procesamos SIEMPRE el soltar tecla: si lo ignoramos cuando el chat tiene el
       foco, se pierde el release y el tanque se queda girando. */
    set_key(data, e->keyval, FALSE);
    return FALSE;
}

/* La ventana pierde el foco (p.ej. Alt-Tab): suelta las teclas para no quedar girando. */
static gboolean on_focus_out(GtkWidget *w, GdkEventFocus *e, gpointer data)
{
    (void)w; (void)e;
    clear_keys((App *)data);
    return FALSE;
}

/* ---------------- Chat ---------------- */

/* Color de cada jugador en el chat: igual que el de su tanque (ver game.c). */
static const char *PLAYER_HEX[MAX_PLAYERS] = {
    "#2EC4B5",  /* cian     (jugador 0) */
    "#F5C740",  /* amarillo (jugador 1) */
    "#A873F2",  /* morado   (jugador 2) */
    "#66D966",  /* verde    (jugador 3) */
};

/* Un mensaje en transito desde el hilo de red hacia la GUI. */
typedef struct {
    App  *a;
    int   sender;
    char  text[CHAT_MAX + 1];
} ChatMsg;

/* Corre en el hilo principal de GTK: agrega la linea al historial y hace scroll. */
static gboolean chat_append_idle(gpointer data)
{
    ChatMsg *m = data;
    App *a = m->a;
    GtkTextView   *view = GTK_TEXT_VIEW(a->chat_view);
    GtkTextBuffer *buf  = gtk_text_view_get_buffer(view);
    GtkTextTagTable *tab = gtk_text_buffer_get_tag_table(buf);
    GtkTextIter end;

    /* Etiqueta de color por jugador (se crea una sola vez y se reutiliza). */
    int k = m->sender & 3;
    char tagname[8];
    snprintf(tagname, sizeof(tagname), "p%d", k);
    GtkTextTag *nametag = gtk_text_tag_table_lookup(tab, tagname);
    if (!nametag)
        nametag = gtk_text_buffer_create_tag(buf, tagname,
                      "foreground", PLAYER_HEX[k],
                      "weight", PANGO_WEIGHT_BOLD, NULL);

    /* "Tu" si es mi propio mensaje; si no, "Jugador N". */
    char name[24];
    if (m->sender == a->local_id) snprintf(name, sizeof(name), "Tu");
    else                          snprintf(name, sizeof(name), "Jugador %d", m->sender);

    /* nombre en color + ":" + mensaje en texto normal + salto de linea */
    gtk_text_buffer_get_end_iter(buf, &end);
    gtk_text_buffer_insert_with_tags(buf, &end, name, -1, nametag, NULL);
    gtk_text_buffer_get_end_iter(buf, &end);
    gtk_text_buffer_insert(buf, &end, ":  ", -1);
    gtk_text_buffer_get_end_iter(buf, &end);
    gtk_text_buffer_insert(buf, &end, m->text, -1);
    gtk_text_buffer_get_end_iter(buf, &end);
    gtk_text_buffer_insert(buf, &end, "\n", -1);

    /* autoscroll al final */
    gtk_text_buffer_get_end_iter(buf, &end);
    GtkTextMark *mk = gtk_text_buffer_create_mark(buf, NULL, &end, FALSE);
    gtk_text_view_scroll_to_mark(view, mk, 0.0, TRUE, 0.0, 1.0);
    gtk_text_buffer_delete_mark(buf, mk);

    g_free(m);
    return G_SOURCE_REMOVE;
}

/* Estilo oscuro del panel de chat (tema visual con CSS). */
static void apply_chat_style(App *a)
{
    GtkCssProvider *css = gtk_css_provider_new();
    gtk_css_provider_load_from_data(css,
        ".chat-view, .chat-view text {"
        "  background-color: #1b1f24;"
        "  color: #dfe3e8;"
        "  font-family: 'Segoe UI','Cantarell',sans-serif;"
        "  font-size: 11pt;"
        "  padding: 6px;"
        "}"
        ".chat-entry {"
        "  background-color: #11151a;"
        "  color: #ffffff;"
        "  caret-color: #ffffff;"
        "  border: 1px solid #323a44;"
        "  padding: 7px;"
        "  font-size: 11pt;"
        "}"
        ".chat-entry:focus { border: 1px solid #2EC4B5; }",
        -1, NULL);

    gtk_style_context_add_class(gtk_widget_get_style_context(a->chat_view),  "chat-view");
    gtk_style_context_add_class(gtk_widget_get_style_context(a->chat_entry), "chat-entry");
    gtk_style_context_add_provider_for_screen(gtk_widget_get_screen(a->chat_view),
        GTK_STYLE_PROVIDER(css), GTK_STYLE_PROVIDER_PRIORITY_APPLICATION);
    g_object_unref(css);
}

/* Lo llama el hilo de red (cliente o host). GTK no es thread-safe, asi que solo
   empaqueta el mensaje y lo manda al hilo principal con g_idle_add. */
static void on_chat_received(int sender, int channel, const char *text, void *user)
{
    (void)channel;
    ChatMsg *m = g_malloc(sizeof(*m));
    m->a = (App *)user;
    m->sender = sender;
    snprintf(m->text, sizeof(m->text), "%s", text);
    g_idle_add(chat_append_idle, m);
}

/* Enter en la caja de texto: envia el mensaje y devuelve el foco al juego. */
static void on_chat_send(GtkEntry *entry, gpointer data)
{
    App *a = data;
    const char *txt = gtk_entry_get_text(entry);
    if (txt && txt[0]) {
        if (a->mode == MODE_HOST)        server_send_chat(a->server, txt);
        else if (a->mode == MODE_CLIENT) client_send_chat(a->client, txt);
        else /* SOLO */                  on_chat_received(a->local_id, CHAT_GENERAL, txt, a);
    }
    gtk_entry_set_text(entry, "");
    gtk_widget_grab_focus(a->canvas);   /* vuelve al juego */
}

/* ---------------- Migracion de host (tolerancia a fallos) ----------------
   Si el host cae, el cliente elige al superviviente con el id mas bajo como
   nuevo host. Ese levanta un servidor; los demas se reconectan a su IP. */
static void do_migration(App *a)
{
    if (a->lost) return;

    PeerInfo peers[MAX_PLAYERS];
    int np   = client_get_peers(a->client, peers, MAX_PLAYERS);
    int myid = client_id(a->client);

    client_close(a->client);    /* cerramos el cliente caido */
    a->client = NULL;

    if (np == 0) {
        a->lost = TRUE;
        fprintf(stderr, "[migracion] sin lista de peers; no se puede continuar\n");
        return;
    }

    /* eleccion: gana el id superviviente mas bajo */
    int new_host = 9999;
    char host_ip[IP_MAX] = "";
    for (int i = 0; i < np; i++)
        if (peers[i].id < new_host) {
            new_host = peers[i].id;
            snprintf(host_ip, IP_MAX, "%s", peers[i].ip);
        }

    fprintf(stderr, "[migracion] host caido -> nuevo host = jugador %d (yo soy %d)\n",
            new_host, myid);

    if (myid == new_host) {
        /* Soy el nuevo host: conservo mi tanque, suelto a los demas (reconectaran)
           y levanto el servidor en el mismo puerto. */
        mutex_lock(&a->lock);
        for (int i = 0; i < MAX_PLAYERS; i++)
            if (i != myid) game_remove_player(&a->gs, i);
        a->gs.local_id = myid;
        mutex_unlock(&a->lock);

        a->local_id = myid;
        a->mode = MODE_HOST;
        a->server = server_start(&a->gs, &a->lock, a->port);
        if (!a->server) { a->lost = TRUE; fprintf(stderr, "[migracion] no pude hospedar\n"); return; }
        server_set_chat_handler(a->server, on_chat_received, a);
        fprintf(stderr, "[migracion] ahora soy el HOST\n");
    } else {
        /* Me reconecto al nuevo host (reintentos: tarda un momento en levantar). */
        Client *nc = NULL;
        for (int t = 0; t < 30 && !nc; t++) {
            nc = client_connect(host_ip, a->port, &a->gs, &a->lock, &a->local_id);
            if (!nc) sleep_ms(150);
        }
        if (!nc) { a->lost = TRUE; fprintf(stderr, "[migracion] no pude reconectar a %s\n", host_ip); return; }
        a->client = nc;
        a->gs.local_id = a->local_id;
        client_set_chat_handler(nc, on_chat_received, a);
        fprintf(stderr, "[migracion] reconectado al nuevo host %s como jugador %d\n",
                host_ip, a->local_id);
    }
}

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
        if (!a->lost && !client_alive(a->client)) {
            do_migration(a);                 /* el host cayo: elegir/reconectar */
        } else if (a->client) {
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

    app.port = port;
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
    app.chat_view  = GTK_WIDGET(gtk_builder_get_object(b, "chat_view"));
    app.chat_entry = GTK_WIDGET(gtk_builder_get_object(b, "chat_entry"));

    const char *title = app.mode == MODE_HOST   ? "Tank Arena — HOST"
                      : app.mode == MODE_CLIENT ? "Tank Arena — CLIENTE"
                                                : "Tank Arena — Local";
    gtk_window_set_title(GTK_WINDOW(win), title);

    /* Estilo visual del chat. */
    apply_chat_style(&app);

    /* Conecta el chat a la red segun el modo. */
    g_signal_connect(app.chat_entry, "activate", G_CALLBACK(on_chat_send), &app);
    if (app.mode == MODE_HOST)        server_set_chat_handler(app.server, on_chat_received, &app);
    else if (app.mode == MODE_CLIENT) client_set_chat_handler(app.client, on_chat_received, &app);

    gtk_widget_add_events(app.canvas, GDK_POINTER_MOTION_MASK |
                                      GDK_BUTTON_PRESS_MASK | GDK_BUTTON_RELEASE_MASK);
    g_signal_connect(app.canvas, "draw",                 G_CALLBACK(on_draw),    &app);
    g_signal_connect(app.canvas, "motion-notify-event",  G_CALLBACK(on_motion),  &app);
    g_signal_connect(app.canvas, "button-press-event",   G_CALLBACK(on_button_press),   &app);
    g_signal_connect(app.canvas, "button-release-event", G_CALLBACK(on_button_release), &app);
    g_signal_connect(win, "key-press-event",   G_CALLBACK(on_key_press),   &app);
    g_signal_connect(win, "key-release-event", G_CALLBACK(on_key_release), &app);
    g_signal_connect(win, "focus-out-event",   G_CALLBACK(on_focus_out),   &app);
    g_signal_connect(win, "destroy",           G_CALLBACK(gtk_main_quit),  NULL);

    gtk_widget_show_all(win);
    gtk_widget_grab_focus(app.canvas);   /* arranca con el foco en el juego */
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
