#ifndef CONFIG_H
#define CONFIG_H

/* 1 = online (necesita adaptador de red), 0 = solo local. Se puede pasar con: make ONLINE=0 */
#ifndef ONLINE
#define ONLINE 1
#endif

/* ---- EDITA ESTAS IPs ANTES DE COMPILAR (separadas por comas) ---- */
#define PS2_IP     192,168,1,77     /* IP fija que tendra la PS2 */
#define PS2_MASK   255,255,255,0
#define PS2_GW     192,168,1,1
#define SERVER_IP  192,168,1,50     /* IP del PC que ejecuta bombserver */

/* ---- Auxiliares ---- */
#define IPU32_(a, b, c, d) ((uint32_t)(a) | ((uint32_t)(b) << 8) | ((uint32_t)(c) << 16) | ((uint32_t)(d) << 24))
#define IPU32(...) IPU32_(__VA_ARGS__)

#endif
