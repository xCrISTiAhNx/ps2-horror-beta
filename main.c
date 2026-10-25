#include <tamtypes.h>
#include <kernel.h>
#include <sifrpc.h>
#include <gsKit.h>
#include <dmaKit.h>

int main(int argc, char *argv[]) {
    // 1. Inicializar el sistema de comunicación de la PS2
    SifInitRpc(0);

    // 2. Configurar la memoria y los canales DMA para enviar gráficos al chip de video (GS)
    dmaKit_init(D_CTRL_RELE_OFF, D_CTRL_MFD_OFF, D_CTRL_STS_UNSPEC,
                D_CTRL_STD_OFF, D_CTRL_RCYC_8, 1 << DMA_CHANNEL_GIF);
    dmaKit_chan_init(DMA_CHANNEL_GIF);

    // 3. Inicializar gsKit (Entorno gráfico)
    GSGLOBAL *gsGlobal = gsKit_init_global();
    gsGlobal->PrimAlphaEnable = GS_SETTING_ON;
    gsKit_init_screen(gsGlobal);

    // 4. Definir colores en formato RGBA (Rojo, Verde, Azul, Transparencia, Z-buffer)
    u64 color_fondo = GS_SETREG_RGBAQ(30, 30, 30, 0, 0); // Gris oscuro
    u64 color_jugador = GS_SETREG_RGBAQ(255, 0, 0, 0, 0); // Rojo puro

    // Variables de posición del jugador (en píxeles)
    float player_x = 100.0f;
    float player_y = 100.0f;
    float player_size = 32.0f;

    // 5. Bucle Principal del Juego (Se ejecuta a 60 cuadros por segundo)
    while (1) {
        // Limpiar la pantalla dibujando el color de fondo
        gsKit_clear(gsGlobal, color_fondo);

        // Dibujar el jugador (un rectángulo de player_size x player_size)
        // Parámetros: entorno, X inicial, Y inicial, X final, Y final, capa (Z), color
        gsKit_prim_sprite(gsGlobal, 
                          player_x, player_y, 
                          player_x + player_size, player_y + player_size, 
                          1, color_jugador);

        // Enviar la lista de comandos gráficos a la GPU de la PS2
        gsKit_queue_exec(gsGlobal);

        // Esperar al refresco de la pantalla y cambiar los buffers gráficos
        gsKit_sync_flip(gsGlobal);
    }

    return 0;
}