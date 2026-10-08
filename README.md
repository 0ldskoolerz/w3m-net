# w3m-net

Suite de herramientas de red para el escritorio
**[W3M](https://github.com/0ldskoolerz/W3M)**, inspirada en el conjunto de
diagnóstico de **Trinux** (la distro floppy de networking de los 2000s):
GUIs Xlib finas, la lógica en busybox/applets CLI. Para administrar
**tu propia red**.

## Aplicaciones

| Binario | Equivalente Trinux/CLI | Qué hace | Motor |
|---|---|---|---|
| `w3m-ping` | ping / traceroute | host + Ping/Traceroute en vivo, scroll, % pérdida | `ping`, `traceroute` |
| `w3m-ifaces` | ifconfig / netcfg | interfaces (-a), DHCP (udhcpc), up/down, escaneo wifi | `ifconfig`/`ip`, `udhcpc`, `iwlist` |
| `w3m-ports` | netstat | puertos abiertos (LISTEN resaltado) y conexiones activas | `netstat`/`ss` |
| `w3m-scan` | nmap / barrido ping | inventario de tu LAN /24: detecta gateway, hosts activos y MACs | `nmap -sn` (o barrido ping si no hay nmap) |
| `w3m-sniff` | tcpdump | captura en vivo con filtro (sintaxis tcpdump), conteo de paquetes | `tcpdump` |
| `w3m-traf` | ntop / iptraf | tasas RX/TX por interfaz en vivo con sparkline de 2 min | `/proc/net/dev` (sin deps) |
| `w3m-nc` | netcat | conectar a host:puerto, enviar/recibir texto — probar tus servicios | sockets directos |
| `w3m-dns` | dig / nslookup | consultas A/AAAA/CNAME/MX/NS/PTR contra tu resolver o uno específico | `dig` (fallback `nslookup`) |
| `w3m-route` | netstat -r / route | tabla de rutas, gateway default, y qué ruta usará una IP destino | `route -n` / `ip route` |
| `w3m-arp` | arp | tabla ARP/vecinos con MACs, refresco por ping, detección de conflictos de IP | `arp` / `ip neigh` |
| `w3m-link` | mii-tool | estado físico: portador, velocidad, dúplex, errores rx/tx | sysfs + `mii-tool` |

## Filosofía (heredada de w3m-apps)

- GUI delgada Xlib, cero `system()` con input del usuario: todo
  `execvp`/pipes con argv controlado
- busybox hace el trabajo; si falta un applet, la app degrada y avisa
  en su barra de estado
- misma estética Win 3.x del WM (bisel, grey70/navyblue)

## Compilar

```sh
sudo pacman -S base-devel libx11
make
./w3m-ping
```

## Instalar en W3M Linux (la distro)

El repo incluye `package/w3m-net.mk` listo para copiar al external tree
de [w3m-linux](https://github.com/0ldskoolerz/w3m-linux):

```sh
cp package/w3m-net.mk /ruta/w3m-linux/package/w3m-net/w3m-net.mk
# añadir a configs/w3m_linux_defconfig:  BR2_PACKAGE_W3M_NET=y
```

Y para que nmap esté disponible en la distro, añade también
`BR2_PACKAGE_NMAP=y` al defconfig.

## Documentación

| Documento | Contenido |
|---|---|
| [docs/MANUAL.md](docs/MANUAL.md) | Manual detallado de cada aplicación: uso, interpretación de resultados, atajos y requisitos |
| [docs/ANALISIS.md](docs/ANALISIS.md) | Metodología de análisis de red completo combinando las 7 apps en 5 fases, con ejemplo real paso a paso |

## El ecosistema

- [W3M](https://github.com/0ldskoolerz/W3M) — el gestor de ventanas
- [w3m-apps](https://github.com/0ldskoolerz/w3m-apps) — apps del
  escritorio (explorador, terminal, tareas...), misma filosofía
- [w3m-linux](https://github.com/0ldskoolerz/w3m-linux) — distro
  completa; w3m-net ya está integrado como paquete (v0.1.0)

## Uso responsable

w3m-scan y w3m-ping son para auditar **tu propia red** (inventario de
dispositivos, diagnóstico). Escanear redes ajenas sin autorización es
ilegal en la mayoría de jurisdicciones. No se incluyen herramientas de
intercepción de credenciales que tenía Trinux (dsniff & co.).
