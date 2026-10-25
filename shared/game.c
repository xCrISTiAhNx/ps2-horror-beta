#include "game.h"
#include <math.h>
#include <string.h>

static uint32_t seed = 2463534242u;
static uint32_t rnd(void) { seed ^= seed << 13; seed ^= seed >> 17; seed ^= seed << 5; return seed; }

/* 4 esquinas + 4 puntos medios de los bordes */
static const uint8_t SPX[MAX_PLAYERS] = {1, 13, 1, 13, 7, 7, 1, 13};
static const uint8_t SPY[MAX_PLAYERS] = {1, 11, 11, 1, 1, 11, 6, 6};

static int solid(uint8_t t) { return t == T_WALL || t == T_BLOCK; }

static void spawn(Game *g, int id)
{
    static const int dx[5] = {0, 1, -1, 0, 0}, dy[5] = {0, 0, 0, 1, -1};
    Player *p = &g->p[id];
    for (int k = 0; k < 5; k++) {
        int x = SPX[id] + dx[k], y = SPY[id] + dy[k];
        if (x > 0 && y > 0 && x < MAP_W - 1 && y < MAP_H - 1 && g->map[y][x] != T_WALL)
            g->map[y][x] = T_EMPTY;
    }
    p->x = SPX[id] + 0.5f;
    p->y = SPY[id] + 0.5f;
    p->ang = atan2f(MAP_H / 2.0f - p->y, MAP_W / 2.0f - p->x);
    p->alive = 1; p->maxBombs = 1; p->range = 2; p->speed = 0; p->prevBomb = 0;
}

static void new_round(Game *g)
{
    memset(g->map, 0, sizeof g->map);
    memset(g->flame, 0, sizeof g->flame);
    memset(g->b, 0, sizeof g->b);
    for (int y = 0; y < MAP_H; y++)
        for (int x = 0; x < MAP_W; x++) {
            if (x == 0 || y == 0 || x == MAP_W - 1 || y == MAP_H - 1 || (x % 2 == 0 && y % 2 == 0))
                g->map[y][x] = T_WALL;
            else if (rnd() % 100 < 65)
                g->map[y][x] = T_BLOCK;
        }
    for (int i = 0; i < MAX_PLAYERS; i++) {
        if (g->p[i].active) spawn(g, i); else g->p[i].alive = 0;
    }
    g->resetTimer = 0;
}

void game_init(Game *g) { memset(g, 0, sizeof *g); new_round(g); }

void game_join(Game *g, int id)
{
    if (id < 0 || id >= MAX_PLAYERS) return;
    g->p[id].active = 1; g->p[id].wins = 0;
    spawn(g, id);
}

void game_leave(Game *g, int id)
{
    if (id < 0 || id >= MAX_PLAYERS) return;
    g->p[id].active = 0; g->p[id].alive = 0;
}

uint8_t game_tile(const Game *g, int x, int y)
{
    if (x < 0 || y < 0 || x >= MAP_W || y >= MAP_H) return T_WALL;
    return g->map[y][x] | (g->flame[y][x] > 0 ? FLAME : 0);
}

static int bomb_at(const Game *g, int cx, int cy)
{
    for (int i = 0; i < MAX_BOMBS; i++)
        if (g->b[i].on && g->b[i].cx == cx && g->b[i].cy == cy) return 1;
    return 0;
}

static int blocked(const Game *g, int id, float nx, float ny)
{
    const float r = 0.25f;
    const Player *p = &g->p[id];
    for (int c = 0; c < 4; c++) {
        float px = nx + ((c & 1) ? r : -r), py = ny + ((c & 2) ? r : -r);
        int cx = (int)px, cy = (int)py;
        if (cx < 0 || cy < 0 || cx >= MAP_W || cy >= MAP_H) return 1;
        if (solid(g->map[cy][cx])) return 1;
        if (bomb_at(g, cx, cy)) {
            /* puedes salir de una bomba en la que ya estas encima */
            if (!(fabsf(p->x - (cx + 0.5f)) < 0.5f + r && fabsf(p->y - (cy + 0.5f)) < 0.5f + r))
                return 1;
        }
    }
    return 0;
}

static void blast(Game *g, int cx, int cy, int range)
{
    static const int DX[4] = {1, -1, 0, 0}, DY[4] = {0, 0, 1, -1};
    g->flame[cy][cx] = FLAME_TIME;
    for (int d = 0; d < 4; d++) {
        for (int k = 1; k <= range; k++) {
            int x = cx + DX[d] * k, y = cy + DY[d] * k;
            uint8_t t = g->map[y][x];
            if (t == T_WALL) break;
            g->flame[y][x] = FLAME_TIME;
            if (t == T_BLOCK) {
                uint32_t r = rnd() % 100;
                g->map[y][x] = r < 12 ? T_PU_RANGE : r < 24 ? T_PU_BOMB : r < 32 ? T_PU_SPEED : T_EMPTY;
                break;
            }
            if (t >= T_PU_RANGE) { g->map[y][x] = T_EMPTY; break; }
            for (int i = 0; i < MAX_BOMBS; i++)
                if (g->b[i].on && g->b[i].cx == x && g->b[i].cy == y) g->b[i].timer = 0; /* reaccion en cadena */
        }
    }
}

void game_step(Game *g, const Input *in, float dt)
{
    g->tick++;

    for (int y = 0; y < MAP_H; y++)
        for (int x = 0; x < MAP_W; x++)
            if (g->flame[y][x] > 0) g->flame[y][x] -= dt;

    /* jugadores */
    for (int i = 0; i < MAX_PLAYERS; i++) {
        Player *p = &g->p[i];
        if (!p->active || !p->alive) continue;

        p->ang += (in[i].turn / 127.0f) * 3.0f * dt;
        float sp = (2.6f + 0.5f * p->speed) * dt;
        float f = in[i].fwd / 127.0f, s = in[i].strafe / 127.0f;
        float dx = (cosf(p->ang) * f - sinf(p->ang) * s) * sp;
        float dy = (sinf(p->ang) * f + cosf(p->ang) * s) * sp;
        if (!blocked(g, i, p->x + dx, p->y)) p->x += dx;
        if (!blocked(g, i, p->x, p->y + dy)) p->y += dy;

        int cx = (int)p->x, cy = (int)p->y;

        /* colocar bomba (flanco de subida) */
        if (in[i].bomb && !p->prevBomb && !bomb_at(g, cx, cy)) {
            int mine = 0, slot = -1;
            for (int k = 0; k < MAX_BOMBS; k++) {
                if (g->b[k].on && g->b[k].owner == i) mine++;
                if (!g->b[k].on && slot < 0) slot = k;
            }
            if (mine < p->maxBombs && slot >= 0)
                g->b[slot] = (Bomb){1, (uint8_t)cx, (uint8_t)cy, (uint8_t)i, p->range, BOMB_TIME};
        }
        p->prevBomb = in[i].bomb;
    }

    /* bombas */
    for (int i = 0; i < MAX_BOMBS; i++) if (g->b[i].on) g->b[i].timer -= dt;
    int again;
    do {
        again = 0;
        for (int i = 0; i < MAX_BOMBS; i++)
            if (g->b[i].on && g->b[i].timer <= 0) {
                g->b[i].on = 0;
                blast(g, g->b[i].cx, g->b[i].cy, g->b[i].range);
                again = 1;
            }
    } while (again);

    /* muerte y power-ups */
    for (int i = 0; i < MAX_PLAYERS; i++) {
        Player *p = &g->p[i];
        if (!p->active || !p->alive) continue;
        int cx = (int)p->x, cy = (int)p->y;
        if (g->flame[cy][cx] > 0) { p->alive = 0; continue; }
        uint8_t t = g->map[cy][cx];
        if (t >= T_PU_RANGE) {
            if (t == T_PU_RANGE && p->range < 9) p->range++;
            if (t == T_PU_BOMB  && p->maxBombs < 8) p->maxBombs++;
            if (t == T_PU_SPEED && p->speed < 5) p->speed++;
            g->map[cy][cx] = T_EMPTY;
        }
    }

    /* rondas */
    int act = 0, alive = 0, last = -1;
    for (int i = 0; i < MAX_PLAYERS; i++)
        if (g->p[i].active) { act++; if (g->p[i].alive) { alive++; last = i; } }
    if (g->resetTimer > 0) {
        g->resetTimer -= dt;
        if (g->resetTimer <= 0) new_round(g);
    } else if ((act >= 2 && alive <= 1) || (act == 1 && alive == 0)) {
        if (act >= 2 && alive == 1) g->p[last].wins++;
        g->resetTimer = 3.0f;
    }
}
