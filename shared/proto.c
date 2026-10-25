#include "proto.h"
#include <math.h>
#include <string.h>

#define PI2 6.28318530718f

static void w16(uint8_t *o, int v) { o[0] = v & 0xFF; o[1] = (v >> 8) & 0xFF; }
static int r16u(const uint8_t *i) { return i[0] | (i[1] << 8); }
static int r16s(const uint8_t *i) { return (int16_t)r16u(i); }

int proto_header(uint8_t *o, uint8_t type, uint8_t id)
{
    w16(o, NET_MAGIC); o[2] = type; o[3] = id;
    return 4;
}

int proto_check(const uint8_t *i, int len, uint8_t *type, uint8_t *id)
{
    if (len < 4 || r16u(i) != NET_MAGIC) return 0;
    *type = i[2]; *id = i[3];
    return 1;
}

int proto_pack_input(uint8_t *o, uint8_t id, const Input *in)
{
    int n = proto_header(o, PKT_INPUT, id);
    o[n++] = (uint8_t)in->fwd; o[n++] = (uint8_t)in->strafe;
    o[n++] = (uint8_t)in->turn; o[n++] = in->bomb;
    return n;
}

int proto_unpack_input(const uint8_t *i, int len, Input *in)
{
    if (len < 8) return 0;
    in->fwd = (int8_t)i[4]; in->strafe = (int8_t)i[5];
    in->turn = (int8_t)i[6]; in->bomb = i[7] ? 1 : 0;
    return 1;
}

/* Estado: hdr(4) tick(4) | 8 x jugador(11) | nb(1) | nb x (cx,cy) | mapa(195) */
int proto_pack_state(const Game *g, uint8_t *o)
{
    int n = proto_header(o, PKT_STATE, 0xFF);
    o[n++] = g->tick & 0xFF; o[n++] = (g->tick >> 8) & 0xFF;
    o[n++] = (g->tick >> 16) & 0xFF; o[n++] = (g->tick >> 24) & 0xFF;

    for (int i = 0; i < MAX_PLAYERS; i++) {
        const Player *p = &g->p[i];
        float t = p->ang / PI2; t -= floorf(t);
        w16(o + n, (int)(p->x * 256.0f)); n += 2;
        w16(o + n, (int)(p->y * 256.0f)); n += 2;
        w16(o + n, (int)(t * 65536.0f));  n += 2;
        o[n++] = (p->active ? 1 : 0) | (p->alive ? 2 : 0);
        o[n++] = p->range; o[n++] = p->maxBombs; o[n++] = p->speed; o[n++] = p->wins;
    }
    int nbPos = n++, nb = 0;
    for (int i = 0; i < MAX_BOMBS; i++)
        if (g->b[i].on) { o[n++] = g->b[i].cx; o[n++] = g->b[i].cy; nb++; }
    o[nbPos] = (uint8_t)nb;
    for (int y = 0; y < MAP_H; y++)
        for (int x = 0; x < MAP_W; x++) o[n++] = game_tile(g, x, y);
    return n;
}

int proto_unpack_state(Game *v, const uint8_t *i, int len)
{
    const int base = 8 + MAX_PLAYERS * 11;
    if (len < base + 1 + MAP_W * MAP_H) return 0;
    v->tick = i[4] | (i[5] << 8) | (i[6] << 16) | ((uint32_t)i[7] << 24);

    const uint8_t *s = i + 8;
    for (int k = 0; k < MAX_PLAYERS; k++, s += 11) {
        Player *p = &v->p[k];
        p->x = r16s(s) / 256.0f; p->y = r16s(s + 2) / 256.0f;
        p->ang = r16u(s + 4) * (PI2 / 65536.0f);
        p->active = s[6] & 1; p->alive = (s[6] >> 1) & 1;
        p->range = s[7]; p->maxBombs = s[8]; p->speed = s[9]; p->wins = s[10];
    }
    int nb = i[base];
    if (len < base + 1 + nb * 2 + MAP_W * MAP_H) return 0;
    memset(v->b, 0, sizeof v->b);
    for (int k = 0; k < nb && k < MAX_BOMBS; k++) {
        v->b[k].on = 1; v->b[k].cx = i[base + 1 + k * 2]; v->b[k].cy = i[base + 2 + k * 2];
    }
    const uint8_t *m = i + base + 1 + nb * 2;
    for (int y = 0; y < MAP_H; y++)
        for (int x = 0; x < MAP_W; x++) {
            uint8_t t = *m++;
            v->map[y][x] = t & 0x7F;
            v->flame[y][x] = (t & FLAME) ? 0.1f : 0.0f;
        }
    return 1;
}
