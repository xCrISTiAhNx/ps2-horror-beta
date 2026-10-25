/* BombFPS - Bomberman en primera persona para PS2 (raycaster + gsKit) */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <tamtypes.h>
#include <kernel.h>
#include <sifrpc.h>
#include <loadfile.h>
#include <iopcontrol.h>
#include <libpad.h>
#include <gsKit.h>
#include <dmaKit.h>

#include "config.h"
#include "game.h"
#if ONLINE
#include "net_ps2.h"
#endif

#define RGB(r, g, b) GS_SETREG_RGBAQ((r), (g), (b), 0x80, 0x00)
#define NR    160          /* columnas del raycaster */
#define FOVP  0.66f        /* longitud del plano de camara */

static GSGLOBAL *gs;
static int SW, SH;
static float zbuf[NR];

static const u8 PCOL[MAX_PLAYERS][3] = {
    {230, 60, 60}, {60, 120, 240}, {60, 200, 80}, {240, 210, 50},
    {200, 80, 220}, {60, 210, 210}, {240, 140, 40}, {235, 235, 235}};

/* ------------------------------------------------------------------ */
/* Dibujo                                                              */
/* ------------------------------------------------------------------ */
static void rect(float x1, float y1, float x2, float y2, u64 c)
{
    if (y1 < 0) y1 = 0;
    if (y2 > SH) y2 = (float)SH;
    if (x1 < 0) x1 = 0;
    if (x2 > SW) x2 = (float)SW;
    if (x2 <= x1 || y2 <= y1) return;
    gsKit_prim_sprite(gs, x1, y1, x2, y2, 1, c);
}

static u64 shade(int r, int g_, int b, float dist, int side)
{
    float f = 1.0f / (1.0f + dist * 0.18f);
    if (side) f *= 0.7f;
    return RGB((int)(r * f), (int)(g_ * f), (int)(b * f));
}

typedef struct { float x, y, d; int kind, a; } Spr; /* kind 0 jugador, 1 bomba, 2 power-up */
static int spr_cmp(const void *a, const void *b)
{
    float da = ((const Spr *)a)->d, db = ((const Spr *)b)->d;
    return (da < db) - (da > db); /* lejos -> cerca */
}

static void render(const Game *g, int me)
{
    const Player *pl = &g->p[me];
    const float K = (SW * 0.5f) / FOVP;
    const int colw = SW / NR;
    float dx = cosf(pl->ang), dy = sinf(pl->ang);
    float plx = -dy * FOVP, ply = dx * FOVP;

    rect(0, 0, SW, SH / 2, RGB(35, 35, 70));
    rect(0, SH / 2, SW, SH, RGB(55, 55, 55));

    /* --- paredes --- */
    for (int c = 0; c < NR; c++) {
        float cam = 2.0f * c / NR - 1.0f;
        float rdx = dx + plx * cam, rdy = dy + ply * cam;
        int mx = (int)pl->x, my = (int)pl->y;
        float ddx = fabsf(1.0f / (rdx == 0 ? 1e-6f : rdx));
        float ddy = fabsf(1.0f / (rdy == 0 ? 1e-6f : rdy));
        int stx, sty; float sdx, sdy;
        if (rdx < 0) { stx = -1; sdx = (pl->x - mx) * ddx; } else { stx = 1; sdx = (mx + 1.0f - pl->x) * ddx; }
        if (rdy < 0) { sty = -1; sdy = (pl->y - my) * ddy; } else { sty = 1; sdy = (my + 1.0f - pl->y) * ddy; }

        int side = 0, hit = 0, guard = 0; uint8_t tile = T_WALL;
        while (!hit && guard++ < 40) {
            if (sdx < sdy) { sdx += ddx; mx += stx; side = 0; }
            else           { sdy += ddy; my += sty; side = 1; }
            tile = game_tile(g, mx, my);
            if ((tile & FLAME) || (tile & 0x7F) == T_WALL || (tile & 0x7F) == T_BLOCK) hit = 1;
        }
        float dist = side == 0 ? sdx - ddx : sdy - ddy;
        if (dist < 0.05f) dist = 0.05f;
        zbuf[c] = dist;
        float lh = K / dist;

        u64 col;
        if (tile & FLAME)               col = RGB(255, 140 + (int)(100 * fabsf(sinf(g->tick * 0.5f))), 20);
        else if (tile == T_BLOCK)       col = shade(170, 105, 50, dist, side);
        else                            col = shade(150, 150, 165, dist, side);
        rect(c * colw, SH / 2 - lh / 2, (c + 1) * colw, SH / 2 + lh / 2, col);
    }

    /* --- sprites --- */
    static Spr spr[MAX_PLAYERS + MAX_BOMBS + MAP_W * MAP_H];
    int ns = 0;
    for (int i = 0; i < MAX_PLAYERS; i++)
        if (i != me && g->p[i].active && g->p[i].alive)
            spr[ns++] = (Spr){g->p[i].x, g->p[i].y, 0, 0, i};
    for (int i = 0; i < MAX_BOMBS; i++)
        if (g->b[i].on) spr[ns++] = (Spr){g->b[i].cx + 0.5f, g->b[i].cy + 0.5f, 0, 1, 0};
    for (int y = 0; y < MAP_H; y++)
        for (int x = 0; x < MAP_W; x++) {
            int t = g->map[y][x];
            if (t >= T_PU_RANGE && t <= T_PU_SPEED) spr[ns++] = (Spr){x + 0.5f, y + 0.5f, 0, 2, t};
        }
    for (int i = 0; i < ns; i++) {
        float sx = spr[i].x - pl->x, sy = spr[i].y - pl->y;
        spr[i].d = sx * sx + sy * sy;
    }
    qsort(spr, ns, sizeof(Spr), spr_cmp);

    float inv = 1.0f / (plx * dy - dx * ply);
    for (int i = 0; i < ns; i++) {
        float sx = spr[i].x - pl->x, sy = spr[i].y - pl->y;
        float tx = inv * (dy * sx - dx * sy);
        float ty = inv * (-ply * sx + plx * sy);
        if (ty < 0.15f) continue;

        float w = spr[i].kind == 0 ? 0.5f : spr[i].kind == 1 ? 0.45f * (1.0f + 0.12f * sinf(g->tick * 0.3f)) : 0.3f;
        float u = K / ty;
        float floorY = SH / 2 + u / 2;
        float scx = (SW / 2.0f) * (1.0f + tx / ty);
        float hw = u * w / 2;
        int c0 = (int)((scx - hw) / colw), c1 = (int)((scx + hw) / colw);

        for (int c = c0; c <= c1; c++) {
            if (c < 0 || c >= NR || ty >= zbuf[c]) continue;
            float xa = c * colw, xb = (c + 1) * colw;
            if (spr[i].kind == 0) {
                const u8 *pc = PCOL[spr[i].a];
                rect(xa, floorY - u * 0.60f, xb, floorY, shade(pc[0], pc[1], pc[2], ty, 0));
                rect(xa, floorY - u * 0.85f, xb, floorY - u * 0.60f, shade(250, 210, 170, ty, 0));
            } else if (spr[i].kind == 1) {
                rect(xa, floorY - u * 0.36f, xb, floorY, shade(25, 25, 30, ty, 0));
                rect(xa, floorY - u * 0.42f, xb, floorY - u * 0.36f, RGB(255, 60, 30));
            } else {
                int t = spr[i].a;
                u64 col = t == T_PU_RANGE ? RGB(255, 90, 40) : t == T_PU_BOMB ? RGB(70, 130, 255) : RGB(80, 230, 100);
                rect(xa, floorY - u * 0.28f, xb, floorY - u * 0.06f, col);
            }
        }
    }

    /* --- HUD: minimapa --- */
    const int cs = 8, ox = 12, oy = 12;
    for (int y = 0; y < MAP_H; y++)
        for (int x = 0; x < MAP_W; x++) {
            uint8_t t = game_tile(g, x, y);
            u64 col = (t & FLAME) ? RGB(255, 160, 30) : t == T_WALL ? RGB(120, 120, 130) :
                      t == T_BLOCK ? RGB(150, 95, 45) : t >= T_PU_RANGE ? RGB(80, 220, 120) : RGB(20, 20, 25);
            rect(ox + x * cs, oy + y * cs, ox + x * cs + cs - 1, oy + y * cs + cs - 1, col);
        }
    for (int i = 0; i < MAX_BOMBS; i++)
        if (g->b[i].on) rect(ox + g->b[i].cx * cs + 2, oy + g->b[i].cy * cs + 2, ox + g->b[i].cx * cs + 6, oy + g->b[i].cy * cs + 6, RGB(255, 255, 255));
    for (int i = 0; i < MAX_PLAYERS; i++)
        if (g->p[i].active && g->p[i].alive) {
            float px = ox + g->p[i].x * cs, py = oy + g->p[i].y * cs;
            rect(px - 2, py - 2, px + 2, py + 2, RGB(PCOL[i][0], PCOL[i][1], PCOL[i][2]));
        }

    /* --- HUD: stats (rojo = alcance, azul = bombas, verde = velocidad) --- */
    for (int k = 0; k < pl->range; k++)    rect(12 + k * 10, SH - 50, 20 + k * 10, SH - 42, RGB(255, 90, 40));
    for (int k = 0; k < pl->maxBombs; k++) rect(12 + k * 10, SH - 36, 20 + k * 10, SH - 28, RGB(70, 130, 255));
    for (int k = 0; k < pl->speed; k++)    rect(12 + k * 10, SH - 22, 20 + k * 10, SH - 14, RGB(80, 230, 100));

    /* --- victorias de todos (barras de colores arriba a la derecha) --- */
    for (int i = 0; i < MAX_PLAYERS; i++)
        for (int k = 0; k < g->p[i].wins && k < 10; k++)
            rect(SW - 20 - k * 8, 12 + i * 10, SW - 14 - k * 8, 18 + i * 10, RGB(PCOL[i][0], PCOL[i][1], PCOL[i][2]));

    /* --- muerto: marco rojo --- */
    if (!pl->alive) {
        u64 r = RGB(170, 0, 0);
        rect(0, 0, SW, 24, r); rect(0, SH - 24, SW, SH, r);
        rect(0, 0, 24, SH, r); rect(SW - 24, 0, SW, SH, r);
    }
}

/* ------------------------------------------------------------------ */
/* Mando                                                               */
/* ------------------------------------------------------------------ */
static char padBuf[256] __attribute__((aligned(64)));

static void pad_wait(void)
{
    int s;
    do { s = padGetState(0, 0); } while (s != PAD_STATE_STABLE && s != PAD_STATE_FINDCTP1 && s != PAD_STATE_DISCONN);
}

static void pad_start(void)
{
    SifLoadModule("rom0:SIO2MAN", 0, NULL);
    SifLoadModule("rom0:PADMAN", 0, NULL);
    padInit(0);
    padPortOpen(0, 0, padBuf);
    pad_wait();
    padSetMainMode(0, 0, PAD_MMODE_DUALSHOCK, PAD_MMODE_LOCK);
    pad_wait();
}

static int dz(int v)
{
    if (v > -24 && v < 24) return 0;
    if (v > 127) v = 127;
    if (v < -127) v = -127;
    return v;
}

static int clamp127(int v) { return v > 127 ? 127 : v < -127 ? -127 : v; }

/* Stick izq: mover/lateral | Stick der o D-pad izq/der: girar | L1/R1: lateral | X/Cuadrado/R2: bomba */
static Input read_input(void)
{
    Input in; memset(&in, 0, sizeof in);
    struct padButtonStatus b;
    int st = padGetState(0, 0);
    if (st != PAD_STATE_STABLE && st != PAD_STATE_FINDCTP1) return in;
    if (padRead(0, 0, &b) == 0) return in;

    u32 pd = 0xFFFF ^ b.btns;
    int fwd = -dz((int)b.ljoy_v - 128), strafe = dz((int)b.ljoy_h - 128), turn = dz((int)b.rjoy_h - 128);
    if (pd & PAD_UP)    fwd = 127;
    if (pd & PAD_DOWN)  fwd = -127;
    if (pd & PAD_LEFT)  turn = -127;
    if (pd & PAD_RIGHT) turn = 127;
    if (pd & PAD_L1)    strafe = -127;
    if (pd & PAD_R1)    strafe = 127;
    in.fwd = (int8_t)clamp127(fwd); in.strafe = (int8_t)clamp127(strafe); in.turn = (int8_t)clamp127(turn);
    in.bomb = (pd & (PAD_CROSS | PAD_SQUARE | PAD_R2)) ? 1 : 0;
    return in;
}

/* ------------------------------------------------------------------ */
int main(void)
{
    SifInitRpc(0);
    while (!SifIopReset("", 0)) {}
    while (!SifIopSync()) {}
    SifInitRpc(0);

    pad_start();
#if ONLINE
    int netok = (net_init() == 0);
#endif

    gs = gsKit_init_global();
    gs->ZBuffering = GS_SETTING_OFF;
    gs->DoubleBuffering = GS_SETTING_ON;
    gs->PrimAlphaEnable = GS_SETTING_OFF;
    dmaKit_init(D_CTRL_RELE_OFF, D_CTRL_MFD_OFF, D_CTRL_STS_UNSPEC, D_CTRL_STD_OFF, D_CTRL_RCYC_8, 1 << DMA_CHANNEL_GIF);
    dmaKit_chan_init(DMA_CHANNEL_GIF);
    gsKit_init_screen(gs);
    gsKit_mode_switch(gs, GS_ONESHOT);
    SW = gs->Width; SH = gs->Height;

    static Game game;   /* modo local: estado real. Online: copia del snapshot */
    static Input all[MAX_PLAYERS];
    game_init(&game);
#if !ONLINE
    game_join(&game, 0);
#endif
    float dt = (gs->Mode == GS_MODE_PAL) ? 0.02f : (1.0f / 60.0f);
    unsigned frame = 0;

    for (;;) {
        Input in = read_input();
        gsKit_clear(gs, RGB(0, 0, 0));

#if ONLINE
        if (netok) {
            net_poll(&game);
            if (net_my_id < 0) { if (frame % 60 == 0) net_send_join(); }
            else net_send_input(&in);
        }
        if (netok && net_my_id >= 0 && net_got_state) {
            render(&game, net_my_id);
        } else {
            /* pantalla de espera: amarillo parpadeante = conectando, rojo = sin red / servidor lleno */
            int blink = (frame / 20) & 1;
            u64 c = (!netok || net_full) ? RGB(160, 20, 20) : (blink ? RGB(200, 170, 20) : RGB(60, 50, 10));
            rect(0, 0, SW, SH, c);
        }
#else
        memset(all, 0, sizeof all);
        all[0] = in;
        game_step(&game, all, dt);
        render(&game, 0);
#endif
        gsKit_queue_exec(gs);
        gsKit_sync_flip(gs);
        frame++;
    }
    return 0;
}
