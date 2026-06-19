#include "protocol.h"
#include <string.h>
#include <math.h>

/* ---------- escritura/lectura primitiva (big-endian) ---------- */

static void pu8(uint8_t *b, int *o, uint8_t v)  { b[(*o)++] = v; }
static void pu16(uint8_t *b, int *o, uint16_t v){ b[(*o)++] = (uint8_t)(v >> 8); b[(*o)++] = (uint8_t)v; }
static void pi16(uint8_t *b, int *o, int v)     { pu16(b, o, (uint16_t)(int16_t)v); }
static void pu32(uint8_t *b, int *o, uint32_t v){
    b[(*o)++] = (uint8_t)(v >> 24); b[(*o)++] = (uint8_t)(v >> 16);
    b[(*o)++] = (uint8_t)(v >> 8);  b[(*o)++] = (uint8_t)v;
}

static uint8_t  gu8(const uint8_t *b, int *o)  { return b[(*o)++]; }
static uint16_t gu16(const uint8_t *b, int *o) { uint16_t v = (uint16_t)((b[*o] << 8) | b[*o + 1]); *o += 2; return v; }
static int16_t  gi16(const uint8_t *b, int *o) { return (int16_t)gu16(b, o); }
static uint32_t gu32(const uint8_t *b, int *o) {
    uint32_t v = ((uint32_t)b[*o] << 24) | ((uint32_t)b[*o+1] << 16) |
                 ((uint32_t)b[*o+2] << 8) | (uint32_t)b[*o+3];
    *o += 4; return v;
}

/* angulos: radianes * 1000 en int16 (rango ~ +-32.7 rad, sobra) */
static int16_t ang_enc(double a) { return (int16_t)lround(a * 1000.0); }
static double  ang_dec(int16_t v){ return v / 1000.0; }

/* ---------- framing ---------- */

int frame_send(sock_t s, uint16_t type, const uint8_t *payload, uint16_t plen)
{
    uint16_t len = (uint16_t)(2 + plen);
    uint8_t hdr[4];
    hdr[0] = (uint8_t)(len >> 8);  hdr[1] = (uint8_t)len;
    hdr[2] = (uint8_t)(type >> 8); hdr[3] = (uint8_t)type;
    if (net_send_all(s, hdr, 4) != 0) return -1;
    if (plen && net_send_all(s, payload, plen) != 0) return -1;
    return 0;
}

int frame_recv(sock_t s, uint16_t *type, uint8_t *payload, int max, int *plen)
{
    uint8_t lb[2];
    int r = net_recv_all(s, lb, 2);
    if (r <= 0) return r;
    int len = (lb[0] << 8) | lb[1];
    if (len < 2 || len > MAX_FRAME) return -1;

    uint8_t tmp[MAX_FRAME];
    r = net_recv_all(s, tmp, len);
    if (r <= 0) return r;

    *type = (uint16_t)((tmp[0] << 8) | tmp[1]);
    int pl = len - 2;
    if (pl > max) pl = max;
    memcpy(payload, tmp + 2, pl);
    *plen = pl;
    return 1;
}

/* ---------- INPUT ---------- */

int enc_input(uint8_t *out, const Input *in)
{
    int o = 0;
    uint8_t m = 0;
    if (in->up)    m |= 1;
    if (in->down)  m |= 2;
    if (in->left)  m |= 4;
    if (in->right) m |= 8;
    if (in->fire)  m |= 16;
    pu8(out, &o, m);
    pi16(out, &o, ang_enc(in->aim));
    return o;
}

void dec_input(const uint8_t *p, int plen, Input *in)
{
    if (plen < 3) { memset(in, 0, sizeof(*in)); return; }
    int o = 0;
    uint8_t m = gu8(p, &o);
    in->up    = (m & 1)  != 0;
    in->down  = (m & 2)  != 0;
    in->left  = (m & 4)  != 0;
    in->right = (m & 8)  != 0;
    in->fire  = (m & 16) != 0;
    in->aim   = ang_dec(gi16(p, &o));
}

/* ---------- STATE (snapshot) ---------- */

int enc_state(uint8_t *out, const GameState *gs)
{
    int o = 0;
    pu32(out, &o, (uint32_t)gs->ticks);
    pi16(out, &o, (int)lround(gs->shake * 100.0));
    pu8(out, &o, (uint8_t)(gs->round_over ? 1 : 0));
    pu8(out, &o, (uint8_t)(gs->round_winner < 0 ? 255 : gs->round_winner));

    int np = 0;
    for (int i = 0; i < MAX_PLAYERS; i++) if (gs->players[i].active) np++;
    pu8(out, &o, (uint8_t)np);
    for (int i = 0; i < MAX_PLAYERS; i++) {
        const Tank *p = &gs->players[i];
        if (!p->active) continue;
        uint8_t flags = 0;
        if (p->alive)      flags |= 1;
        if (p->on_foot)    flags |= 2;
        if (p->eliminated) flags |= 4;
        pu8(out, &o, (uint8_t)i);
        pu8(out, &o, flags);
        pi16(out, &o, (int)lround(p->x));
        pi16(out, &o, (int)lround(p->y));
        pi16(out, &o, ang_enc(p->body_angle));
        pi16(out, &o, ang_enc(p->turret_angle));
        pi16(out, &o, p->hp);
        pu16(out, &o, (uint16_t)p->score);
        pu8(out, &o, (uint8_t)lround(p->cr * 255.0));
        pu8(out, &o, (uint8_t)lround(p->cg * 255.0));
        pu8(out, &o, (uint8_t)lround(p->cb * 255.0));
        for (int c = 0; c < NAME_MAX; c++) pu8(out, &o, (uint8_t)p->name[c]);
    }

    pu8(out, &o, (uint8_t)gs->enemy_count);
    for (int e = 0; e < gs->enemy_count; e++) {
        const Tank *en = &gs->enemies[e];
        pu8(out, &o, (uint8_t)(en->alive ? 1 : 0));
        pi16(out, &o, (int)lround(en->x));
        pi16(out, &o, (int)lround(en->y));
        pi16(out, &o, ang_enc(en->turret_angle));
    }

    int nb = 0;
    for (int i = 0; i < MAX_BULLETS; i++) if (gs->bullets[i].active) nb++;
    pu8(out, &o, (uint8_t)nb);
    for (int i = 0; i < MAX_BULLETS; i++) {
        const Bullet *b = &gs->bullets[i];
        if (!b->active) continue;
        pi16(out, &o, (int)lround(b->x));
        pi16(out, &o, (int)lround(b->y));
        pu8(out, &o, (uint8_t)(b->owner < 0 ? 255 : b->owner));
    }

    int nbl = 0;
    for (int i = 0; i < MAX_BLASTS; i++) if (gs->blasts[i].active) nbl++;
    pu8(out, &o, (uint8_t)nbl);
    for (int i = 0; i < MAX_BLASTS; i++) {
        const Blast *bl = &gs->blasts[i];
        if (!bl->active) continue;
        pi16(out, &o, (int)lround(bl->x));
        pi16(out, &o, (int)lround(bl->y));
        pu8(out, &o, (uint8_t)bl->t);
        pu8(out, &o, (uint8_t)bl->maxt);
        pu8(out, &o, (uint8_t)lround(bl->scale * 10.0));
    }

    return o;
}

void dec_state(const uint8_t *p, int plen, GameState *gs)
{
    if (plen < 5) return;
    int o = 0;
    gs->ticks = gu32(p, &o);
    gs->shake = gi16(p, &o) / 100.0;
    gs->round_over = gu8(p, &o);
    { uint8_t w = gu8(p, &o); gs->round_winner = (w == 255) ? -1 : w; }

    for (int i = 0; i < MAX_PLAYERS; i++) {
        gs->players[i].active = false; gs->players[i].alive = false;
        gs->players[i].on_foot = false; gs->players[i].eliminated = false;
    }
    int np = gu8(p, &o);
    for (int k = 0; k < np; k++) {
        int id = gu8(p, &o);
        uint8_t flags = gu8(p, &o);
        int16_t x  = gi16(p, &o), y = gi16(p, &o);
        int16_t ba = gi16(p, &o), ta = gi16(p, &o);
        int16_t hp = gi16(p, &o);
        uint16_t sc = gu16(p, &o);
        uint8_t cr = gu8(p, &o), cg = gu8(p, &o), cb = gu8(p, &o);
        char nm[NAME_MAX];
        for (int c = 0; c < NAME_MAX; c++) nm[c] = (char)gu8(p, &o);
        nm[NAME_MAX - 1] = '\0';
        if (id < 0 || id >= MAX_PLAYERS) continue;
        Tank *t = &gs->players[id];
        t->active = true;
        t->alive      = (flags & 1) != 0;
        t->on_foot    = (flags & 2) != 0;
        t->eliminated = (flags & 4) != 0;
        t->x = x; t->y = y;
        t->body_angle = ang_dec(ba);
        t->turret_angle = ang_dec(ta);
        t->hp = hp; t->score = sc;
        if (cr || cg || cb) { t->cr = cr / 255.0; t->cg = cg / 255.0; t->cb = cb / 255.0; }
        else                  game_set_player_visual(t, id);   /* sin color: por defecto */
        memcpy(t->name, nm, NAME_MAX);
    }

    int ne = gu8(p, &o);
    if (ne > MAX_ENEMIES) ne = MAX_ENEMIES;
    gs->enemy_count = ne;
    for (int e = 0; e < ne; e++) {
        uint8_t alive = gu8(p, &o);
        int16_t x = gi16(p, &o), y = gi16(p, &o), ta = gi16(p, &o);
        Tank *en = &gs->enemies[e];
        en->alive = alive != 0;
        en->x = x; en->y = y;
        en->turret_angle = ang_dec(ta);
        game_set_enemy_visual(en);
    }

    for (int i = 0; i < MAX_BULLETS; i++) gs->bullets[i].active = false;
    int nb = gu8(p, &o);
    if (nb > MAX_BULLETS) nb = MAX_BULLETS;
    for (int k = 0; k < nb; k++) {
        int16_t x = gi16(p, &o), y = gi16(p, &o);
        uint8_t ow = gu8(p, &o);
        Bullet *b = &gs->bullets[k];
        b->active = true;
        b->x = x; b->y = y;
        b->owner = (ow == 255) ? -1 : ow;
    }

    for (int i = 0; i < MAX_BLASTS; i++) gs->blasts[i].active = false;
    int nbl = gu8(p, &o);
    if (nbl > MAX_BLASTS) nbl = MAX_BLASTS;
    for (int k = 0; k < nbl; k++) {
        int16_t x = gi16(p, &o), y = gi16(p, &o);
        uint8_t t = gu8(p, &o), mt = gu8(p, &o), sc = gu8(p, &o);
        Blast *bl = &gs->blasts[k];
        bl->active = true;
        bl->x = x; bl->y = y;
        bl->t = t; bl->maxt = mt; bl->scale = sc / 10.0;
    }
}

/* ---------- CHAT ---------- */

int enc_chat(uint8_t *out, int sender, int channel, const char *text)
{
    int o = 0;
    int n = (int)strlen(text);
    if (n > CHAT_MAX) n = CHAT_MAX;
    pu8(out, &o, (uint8_t)sender);
    pu8(out, &o, (uint8_t)channel);
    pu8(out, &o, (uint8_t)n);
    memcpy(out + o, text, n);
    o += n;
    return o;
}

int dec_chat(const uint8_t *p, int plen, int *sender, int *channel,
             char *text, int maxlen)
{
    if (maxlen > 0) text[0] = '\0';
    if (plen < 3 || maxlen < 1) return 0;
    int o = 0;
    int s  = gu8(p, &o);
    int ch = gu8(p, &o);
    int n  = gu8(p, &o);
    if (n > plen - 3)   n = plen - 3;    /* no leer fuera del payload */
    if (n > maxlen - 1) n = maxlen - 1;  /* no desbordar el buffer destino */
    if (n < 0) n = 0;
    memcpy(text, p + o, n);
    text[n] = '\0';
    if (sender)  *sender  = s;
    if (channel) *channel = ch;
    return n;
}

/* ---------- PROFILE (nombre + color) ---------- */

int enc_profile(uint8_t *out, const char *name, double r, double g, double b)
{
    int o = 0;
    pu8(out, &o, (uint8_t)lround(r * 255.0));
    pu8(out, &o, (uint8_t)lround(g * 255.0));
    pu8(out, &o, (uint8_t)lround(b * 255.0));
    int n = (int)strlen(name);
    if (n > NAME_MAX - 1) n = NAME_MAX - 1;
    memcpy(out + o, name, n);
    o += n;
    return o;
}

void dec_profile(const uint8_t *p, int plen, char *name, int maxlen,
                 double *r, double *g, double *b)
{
    if (maxlen > 0) name[0] = '\0';
    if (plen < 3) return;
    int o = 0;
    uint8_t cr = gu8(p, &o), cg = gu8(p, &o), cb = gu8(p, &o);
    if (r) *r = cr / 255.0;
    if (g) *g = cg / 255.0;
    if (b) *b = cb / 255.0;
    int n = plen - 3;
    if (n > maxlen - 1) n = maxlen - 1;
    if (n < 0) n = 0;
    memcpy(name, p + o, n);
    name[n] = '\0';
}

/* ---------- PEERS ---------- */

int enc_peers(uint8_t *out, const PeerInfo *peers, int n)
{
    int o = 0;
    if (n > 255) n = 255;
    pu8(out, &o, (uint8_t)n);
    for (int i = 0; i < n; i++) {
        int len = (int)strlen(peers[i].ip);
        if (len > IP_MAX - 1) len = IP_MAX - 1;
        pu8(out, &o, (uint8_t)peers[i].id);
        pu8(out, &o, (uint8_t)len);
        memcpy(out + o, peers[i].ip, len);
        o += len;
    }
    return o;
}

int dec_peers(const uint8_t *p, int plen, PeerInfo *out, int max)
{
    if (plen < 1) return 0;
    int o = 0;
    int n = gu8(p, &o);
    int got = 0;
    for (int i = 0; i < n && got < max; i++) {
        if (o + 2 > plen) break;
        int id  = gu8(p, &o);
        int len = gu8(p, &o);
        if (len > IP_MAX - 1) len = IP_MAX - 1;
        if (o + len > plen) break;
        out[got].id = id;
        memcpy(out[got].ip, p + o, len);
        out[got].ip[len] = '\0';
        o += len;
        got++;
    }
    return got;
}
