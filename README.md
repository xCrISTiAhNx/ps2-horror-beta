# BombFPS — Bomberman en primera persona para PS2 (online hasta 8 jugadores)

> **Estado honesto:** el código está escrito pero **no ha sido compilado ni probado** (el entorno donde se generó no tiene el toolchain de PS2). Es normal que al primer `make` aparezcan errores por diferencias de versión de ps2sdk. Pega aquí los errores y se corrigen.

## Estructura

```
bombfps/
├── Makefile            Compila el .elf de PS2
├── shared/             Lógica común (cliente y servidor)
│   ├── game.c/h        Mapa, movimiento, bombas, explosiones, rondas
│   └── proto.c/h       Protocolo de red (UDP, serialización manual)
├── src/                Cliente PS2
│   ├── main.c          Raycaster 1ª persona, mando, bucle principal
│   ├── net_ps2.c/h     DEV9/SMAP/ps2ip + sockets UDP
│   └── config.h        ← EDITA aquí las IPs
└── server/server.c     Servidor autoritativo (Windows o Linux)
```

## Arquitectura online

- El **servidor** (en tu PC) simula todo a 60 Hz y envía el estado a 30 Hz (~420 bytes) a los 8 clientes.
- Cada **PS2** solo envía sus inputs (8 bytes) y dibuja lo que recibe.
- No hay predicción del lado cliente: en LAN se siente bien; por internet habrá retardo.

---

## PASO A PASO desde Windows 8

PS2DEV no tiene instalador oficial para Windows 8, y Docker Desktop y las versiones recientes de MSYS2 exigen Windows 10. La vía fiable es una **máquina virtual Linux**.

### 1. Instalar VirtualBox
- Descarga **VirtualBox 6.1.x** (la rama 7.x puede no soportar Windows 8; comprueba la página de descargas). Instala con las opciones por defecto.
- Activa la virtualización (VT-x / AMD-V) en la BIOS si está desactivada.

### 2. Crear la VM Ubuntu
- Descarga **Ubuntu 22.04 LTS** (ISO de escritorio, 64 bits).
- En VirtualBox: *Nueva* → Linux / Ubuntu 64-bit → **4 GB RAM** (mínimo 2), **30 GB** de disco.
- *Configuración → Red*: cambia el adaptador a **Adaptador puente** (así la VM tiene IP en tu LAN y la PS2 puede alcanzar el servidor).
- Arranca con la ISO e instala Ubuntu.

### 3. Instalar Docker dentro de Ubuntu
Abre una terminal en Ubuntu:

```bash
sudo apt update
sudo apt install -y docker.io build-essential zip
sudo usermod -aG docker $USER
# cierra sesión y vuelve a entrar para aplicar el grupo
docker pull ps2dev/ps2dev:latest
```

### 4. Pasar el proyecto a la VM
Elige una:
- **Carpeta compartida:** VirtualBox → *Dispositivos → Carpetas compartidas*, añade la carpeta de Windows con `bombfps` (automontar). Luego en Ubuntu: `sudo usermod -aG vboxsf $USER` y reinicia sesión; estará en `/media/sf_<nombre>`.
- **Arrastrar y soltar** (requiere *Guest Additions*: *Dispositivos → Insertar imagen de Guest Additions*).
- O copia el `.zip` con un pendrive/USB de VirtualBox.

Copia la carpeta a tu home para evitar problemas de permisos: `cp -r /media/sf_xxx/bombfps ~/bombfps`

### 5. Editar las IPs
Abre `src/config.h` y ajusta:
- `PS2_IP` / `PS2_MASK` / `PS2_GW`: IP libre de tu LAN para la PS2.
- `SERVER_IP`: la IP **de la VM** (ejecuta `ip a` en Ubuntu; con adaptador puente es una IP de tu LAN).

### 6. Compilar el .elf
```bash
cd ~/bombfps
# Primero prueba sin red (más fácil de depurar):
docker run --rm -v "$PWD":/src -w /src ps2dev/ps2dev:latest make ONLINE=0
# Si compila, versión online:
docker run --rm -v "$PWD":/src -w /src ps2dev/ps2dev:latest make clean
docker run --rm -v "$PWD":/src -w /src ps2dev/ps2dev:latest make
```
Resultado: `bombfps.elf` en la carpeta. Los ficheros pueden quedar como root; si hace falta: `sudo chown -R $USER .`

### 7. Compilar y lanzar el servidor
En la misma VM:
```bash
gcc -O2 -Ishared -o bombserver server/server.c shared/game.c shared/proto.c -lm
./bombserver
```
Debe mostrar `BombFPS server escuchando en UDP 28000`. Abre el puerto si usas firewall: `sudo ufw allow 28000/udp`.

> Alternativa en Windows con MinGW: `gcc -O2 -Ishared -o bombserver.exe server/server.c shared/game.c shared/proto.c -lws2_32` y permitir UDP 28000 en el Firewall de Windows.

### 8. Ejecutar en la PS2
Necesitas **adaptador de red** (o PS2 slim con red integrada) y una forma de lanzar ELF (FreeMCBoot/uLaunchELF, OPL, o similar). Copia `bombfps.elf` a un pendrive USB o a la memory card y ábrelo desde uLaunchELF. Conecta la PS2 por cable al mismo router que el servidor.

- Pantalla **amarilla parpadeante**: conectando al servidor.
- Pantalla **roja**: sin red o servidor lleno (8/8).
- Si ves el mundo 3D: ya estás dentro.

### 9. Probar en emulador (opcional)
En Windows 8 solo funcionan versiones antiguas de PCSX2 (p. ej. 1.6.0). El plugin **DEV9 en modo Sockets/PCAP** es delicado; si falla, prueba primero `ONLINE=0` en el emulador y la parte online en consola real.

---

## Controles

| Acción | Control |
|---|---|
| Avanzar / lateral | Stick izquierdo |
| Girar | Stick derecho, o D-pad izquierda/derecha |
| Avanzar/retroceder | D-pad arriba/abajo |
| Lateral | L1 / R1 |
| Poner bomba | ✕, □ o R2 |

**HUD:** minimapa arriba a la izquierda, barras abajo a la izquierda (rojo = alcance, azul = nº bombas, verde = velocidad), victorias por jugador arriba a la derecha, marco rojo = has muerto.
**Power-ups:** naranja = alcance, azul = más bombas, verde = velocidad.

## Limitaciones conocidas / siguientes pasos
1. Sin texto en pantalla (no hay fuente cargada). Siguiente paso: gsKit fontm.
2. Sin texturas: paredes de color plano con sombreado por distancia.
3. Sin predicción ni interpolación cliente.
4. Sin sonido, menú ni selección de servidor (IP fija en `config.h`).
5. Las APIs de `netman`/`ps2ip` varían entre versiones de ps2sdk; es el punto más probable de errores de compilación.
