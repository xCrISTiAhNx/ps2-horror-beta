#ifndef NET_PS2_H
#define NET_PS2_H
#include "game.h"

extern int net_my_id;     /* -1 hasta que el servidor confirma el JOIN */
extern int net_got_state; /* 1 cuando ya llego al menos un snapshot */
extern int net_full;      /* 1 si el servidor respondio "lleno" */

int  net_init(void);
void net_send_join(void);
void net_send_input(const Input *in);
void net_send_leave(void);
void net_poll(Game *view);

#endif
