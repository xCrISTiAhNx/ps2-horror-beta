/* NOTA: las APIs de ps2sdk (netman/ps2ip) cambian entre versiones.
 * Si algo no compila, pega el error y lo ajustamos a tu version. */
#include <tamtypes.h>
#include <kernel.h>
#include <sifrpc.h>
#include <loadfile.h>
#include <stdio.h>
#include <string.h>
#include <netman.h>
#include <ps2ip.h>
#include <lwip/sockets.h>

#include "config.h"
#include "net_ps2.h"
#include "proto.h"

extern unsigned char ps2dev9_irx[]; extern unsigned int size_ps2dev9_irx;
extern unsigned char netman_irx[];  extern unsigned int size_netman_irx;
extern unsigned char smap_irx[];    extern unsigned int size_smap_irx;

#define IP4V(p, ...) IP4_ADDR(p, __VA_ARGS__)

int net_my_id = -1;
int net_got_state = 0;
int net_full = 0;

static int sock = -1;
static struct sockaddr_in srv;

int net_init(void)
{
    int ret;
    SifExecModuleBuffer(ps2dev9_irx, size_ps2dev9_irx, 0, NULL, &ret);
    SifExecModuleBuffer(netman_irx, size_netman_irx, 0, NULL, &ret);
    SifExecModuleBuffer(smap_irx, size_smap_irx, 0, NULL, &ret);

    if (NetManInit() < 0) return -1;

    ip4_addr_t ip, nm, gw;
    IP4V(&ip, PS2_IP);
    IP4V(&nm, PS2_MASK);
    IP4V(&gw, PS2_GW);
    ps2ipInit(&ip, &nm, &gw);

    sock = lwip_socket(AF_INET, SOCK_DGRAM, 0);
    if (sock < 0) return -2;
    int nb = 1;
    lwip_ioctl(sock, FIONBIO, &nb);

    memset(&srv, 0, sizeof srv);
    srv.sin_len = sizeof srv;
    srv.sin_family = AF_INET;
    srv.sin_port = (uint16_t)(((NET_PORT & 0xFF) << 8) | (NET_PORT >> 8)); /* orden de red */
    srv.sin_addr.s_addr = IPU32(SERVER_IP);
    return 0;
}

static void send_pkt(const uint8_t *b, int n)
{
    if (sock >= 0)
        lwip_sendto(sock, b, n, 0, (struct sockaddr *)&srv, sizeof srv);
}

void net_send_join(void)
{
    uint8_t b[8]; int n = proto_header(b, PKT_JOIN, 0);
    send_pkt(b, n);
}

void net_send_leave(void)
{
    uint8_t b[8]; int n = proto_header(b, PKT_LEAVE, (uint8_t)(net_my_id < 0 ? 0 : net_my_id));
    send_pkt(b, n);
}

void net_send_input(const Input *in)
{
    uint8_t b[16]; int n = proto_pack_input(b, (uint8_t)net_my_id, in);
    send_pkt(b, n);
}

void net_poll(Game *view)
{
    static uint8_t buf[MAX_PKT];
    if (sock < 0) return;
    for (;;) {
        int n = lwip_recvfrom(sock, buf, sizeof buf, 0, NULL, NULL);
        if (n <= 0) break;
        uint8_t type, id;
        if (!proto_check(buf, n, &type, &id)) continue;
        if (type == PKT_JOIN) net_my_id = id;
        else if (type == PKT_FULL) net_full = 1;
        else if (type == PKT_STATE && proto_unpack_state(view, buf, n)) net_got_state = 1;
    }
}
