#ifndef PROTO_H
#define PROTO_H
#include <stdint.h>
#include "game.h"

#define NET_PORT  28000
#define NET_MAGIC 0xB0B1
#define MAX_PKT   1024

enum { PKT_JOIN = 1, PKT_INPUT, PKT_STATE, PKT_LEAVE, PKT_FULL };

/* Todos los paquetes: [magic u16 LE][type u8][id u8][payload...] */
int proto_header(uint8_t *o, uint8_t type, uint8_t id);
int proto_check(const uint8_t *i, int len, uint8_t *type, uint8_t *id);

int proto_pack_input(uint8_t *o, uint8_t id, const Input *in);
int proto_unpack_input(const uint8_t *i, int len, Input *in); /* 1 = ok */

int proto_pack_state(const Game *g, uint8_t *o);               /* devuelve bytes */
int proto_unpack_state(Game *view, const uint8_t *i, int len); /* 1 = ok */

#endif
