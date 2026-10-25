/* Servidor autoritativo de BombFPS. 60 Hz de simulacion, 30 Hz de snapshots. */
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "game.h"
#include "proto.h"

#ifdef _WIN32
  #include <winsock2.h>
  typedef int socklen_t;
  static uint32_t now_ms(void) { return (uint32_t)GetTickCount(); }
  static void sleep_ms(int ms) { Sleep(ms); }
  static void set_nonblock(int s) { u_long one = 1; ioctlsocket(s, FIONBIO, &one); }
#else
  #include <sys/socket.h>
  #include <netinet/in.h>
  #include <arpa/inet.h>
  #include <unistd.h>
  #include <fcntl.h>
  #include <time.h>
  static uint32_t now_ms(void) { struct timespec t; clock_gettime(CLOCK_MONOTONIC, &t); return (uint32_t)(t.tv_sec * 1000 + t.tv_nsec / 1000000); }
  static void sleep_ms(int ms) { usleep(ms * 1000); }
  static void set_nonblock(int s) { fcntl(s, F_SETFL, fcntl(s, F_GETFL, 0) | O_NONBLOCK); }
#endif

typedef struct { int used; struct sockaddr_in a; uint32_t last; } Client;
static Client cl[MAX_PLAYERS];
static Game g;
static Input in[MAX_PLAYERS];
static int sock;

static int find_client(const struct sockaddr_in *a)
{
    for (int i = 0; i < MAX_PLAYERS; i++)
        if (cl[i].used && cl[i].a.sin_addr.s_addr == a->sin_addr.s_addr && cl[i].a.sin_port == a->sin_port)
            return i;
    return -1;
}

static void send_to(const struct sockaddr_in *a, const uint8_t *buf, int n)
{
    sendto(sock, (const char *)buf, n, 0, (const struct sockaddr *)a, sizeof *a);
}

static void drop_client(int i, const char *why)
{
    printf("[-] jugador %d fuera (%s)\n", i, why);
    cl[i].used = 0; memset(&in[i], 0, sizeof in[i]);
    game_leave(&g, i);
}

int main(void)
{
#ifdef _WIN32
    WSADATA w; WSAStartup(MAKEWORD(2, 2), &w);
#endif
    sock = (int)socket(AF_INET, SOCK_DGRAM, 0);
    struct sockaddr_in me; memset(&me, 0, sizeof me);
    me.sin_family = AF_INET; me.sin_addr.s_addr = htonl(INADDR_ANY); me.sin_port = htons(NET_PORT);
    if (bind(sock, (struct sockaddr *)&me, sizeof me) < 0) { perror("bind"); return 1; }
    set_nonblock(sock);
    printf("BombFPS server escuchando en UDP %d\n", NET_PORT);

    game_init(&g);
    uint8_t buf[MAX_PKT];
    const double TICK = 1000.0 / 60.0;
    double acc = 0; uint32_t prev = now_ms(); int tickc = 0;

    for (;;) {
        /* ---- recibir ---- */
        for (;;) {
            struct sockaddr_in from; socklen_t fl = sizeof from;
            int n = (int)recvfrom(sock, (char *)buf, sizeof buf, 0, (struct sockaddr *)&from, &fl);
            if (n <= 0) break;
            uint8_t type, id;
            if (!proto_check(buf, n, &type, &id)) continue;
            int slot = find_client(&from);

            if (type == PKT_JOIN) {
                if (slot < 0) {
                    for (int i = 0; i < MAX_PLAYERS; i++) if (!cl[i].used) { slot = i; break; }
                    if (slot < 0) { int m = proto_header(buf, PKT_FULL, 0); send_to(&from, buf, m); continue; }
                    cl[slot].used = 1; cl[slot].a = from;
                    memset(&in[slot], 0, sizeof in[slot]);
                    game_join(&g, slot);
                    printf("[+] jugador %d dentro (%s:%d)\n", slot, inet_ntoa(from.sin_addr), ntohs(from.sin_port));
                }
                cl[slot].last = now_ms();
                int m = proto_header(buf, PKT_JOIN, (uint8_t)slot); send_to(&from, buf, m);
            } else if (slot >= 0) {
                cl[slot].last = now_ms();
                if (type == PKT_INPUT) proto_unpack_input(buf, n, &in[slot]);
                else if (type == PKT_LEAVE) drop_client(slot, "salio");
            }
        }

        /* ---- simular ---- */
        uint32_t t = now_ms(); acc += (t - prev); prev = t;
        if (acc > 200) acc = 200;
        while (acc >= TICK) {
            game_step(&g, in, 1.0f / 60.0f);
            acc -= TICK; tickc++;
            if (tickc % 2 == 0) {
                int n = proto_pack_state(&g, buf);
                for (int i = 0; i < MAX_PLAYERS; i++) if (cl[i].used) send_to(&cl[i].a, buf, n);
            }
            if (tickc % 60 == 0)
                for (int i = 0; i < MAX_PLAYERS; i++)
                    if (cl[i].used && now_ms() - cl[i].last > 5000) drop_client(i, "timeout");
        }
        sleep_ms(1);
    }
}
