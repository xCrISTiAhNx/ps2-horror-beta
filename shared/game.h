#ifndef GAME_H
#define GAME_H
#include <stdint.h>

#define MAP_W 15
#define MAP_H 13
#define MAX_PLAYERS 8
#define MAX_BOMBS 64

#define T_EMPTY    0
#define T_WALL     1
#define T_BLOCK    2
#define T_PU_RANGE 3
#define T_PU_BOMB  4
#define T_PU_SPEED 5
#define FLAME      0x80   /* bit extra solo en game_tile() y en red */

#define BOMB_TIME  2.5f
#define FLAME_TIME 0.6f

typedef struct { int8_t fwd, strafe, turn; uint8_t bomb; } Input;

typedef struct {
    float x, y, ang;
    uint8_t active, alive, maxBombs, range, speed, wins, prevBomb;
} Player;

typedef struct { uint8_t on, cx, cy, owner, range; float timer; } Bomb;

typedef struct {
    uint8_t map[MAP_H][MAP_W];
    float   flame[MAP_H][MAP_W];
    Player  p[MAX_PLAYERS];
    Bomb    b[MAX_BOMBS];
    float   resetTimer;
    uint32_t tick;
} Game;

void    game_init(Game *g);
void    game_join(Game *g, int id);
void    game_leave(Game *g, int id);
void    game_step(Game *g, const Input *in /*[MAX_PLAYERS]*/, float dt);
uint8_t game_tile(const Game *g, int x, int y); /* tile | FLAME si hay fuego */

#endif
