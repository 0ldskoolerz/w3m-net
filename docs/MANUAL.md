# w3m-net — Manual de las aplicaciones

Guía de uso de cada herramienta de la suite. Todas comparten la estética
Win 3.x y el mismo modelo: escribes/eliges, pulsas el botón, el motor
CLI (busybox/tcpdump/nmap) hace el trabajo y la salida aparece en la
ventana con scroll.

---

## w3m-ping — Ping y Traceroute

**Para qué**: medir latencia, verificar que un host responde, y ver la
ruta que siguen tus paquetes (cuántos saltos, dónde se pierden).

**Uso:**
1. Escribe el host en el campo (IP o dominio: `8.8.8.8`, `router.local`)
2. **Enter** o botón **Ping** → 4 paquetes ICMP, muestra RTT por línea
3. Botón **Traceroute** → ruta completa con latencia por salto
4. **Stop** detiene a mitad (traceroute puede tardar)

**Interpretación:**
- La línea final `X packets transmitted, Y received, Z% packet loss`
  aparece también en el badge superior (`pérdida: Z%`)
- Pérdida 0% y RTT estable → enlace sano
- RTT creciente salto a salto → normal; un salto con `* * *` → ese
  router no responde a traceroute (común, no siempre es fallo)
- `100% packet loss` → host caído o ICMP bloqueado (confirma con
  w3m-nc al puerto 80/443 antes de darlo por muerto)

**Atajos**: Enter = Ping · Backspace = borrar

---

## w3m-ifaces — Interfaces de red

**Para qué**: ver el estado de tus tarjetas (IP, MAC, estado), pedir
DHCP, subir/bajar una interfaz, escanear redes wifi.

**Uso:**
1. Al abrir lista todas las interfaces (`ifconfig -a`): botones arriba
   por cada una; clic para **seleccionar** (queda presionada)
2. **DHCP** → `udhcpc` en la interfaz seleccionada (pide IP nueva)
3. **Up** / **Down** → activa/desactiva la interfaz
4. **Escanear wifi** → redes cercanas con calidad y ESSID
5. **Refrescar** → releer estado

**Interpretación:**
- Línea `UP BROADCAST RUNNING MULTICAST` con `inet addr:` = sana
- `inet addr:0.0.0.0` o sin `inet` → sin IP: pulsa DHCP
- En wifi: `Quality:X/70` alto = buena señal; compara ESSIDs y elige

**Nota**: DHCP puede tardar ~5 s; el resultado se refleja tras
refrescar automáticamente.

---

## w3m-ports — Puertos y conexiones

**Para qué**: saber qué servicios escuchan en tu máquina y qué
conexiones salientes/entrantes hay activas.

**Uso:**
1. **Puertos abiertos** (modo por defecto) → `netstat -tulpn`: sockets
   en LISTEN (resaltados en azul), con el proceso dueño si eres root
2. **Conexiones** → solo TCP establecidas/esperando
3. **Refrescar** o tecla **F5/r**

**Interpretación:**
- Cada línea azul (LISTEN) es una puerta abierta de tu máquina.
  Anota el proceso de la última columna: ¿debería estar ahí?
- `0.0.0.0:22` = escucha en todas las interfaces (accesible desde la
  red); `127.0.0.1:8080` = solo local — más seguro
- En Conexiones, la columna `Foreign Address` te dice a quién estás
  conectado; destinos inesperados merecen una mirada

**Cadena útil**: ver algo raro en un LISTEN → abre **w3m-nc** y conéctate
tú mismo a ese puerto para ver qué responde.

---

## w3m-scan — Inventario de tu LAN

**Para qué**: saber qué dispositivos hay vivos en tu red (la /24 del
gateway): IPs, nombres, MACs.

**Uso:**
1. **Escanear LAN** — detecta el gateway automáticamente, escanea la
   subred completa
2. Con nmap instalado: `nmap -sn` con IPs, MACs y fabricantes
3. Sin nmap: barrido ping con busybox (solo "up")

**Interpretación:**
- Compara la lista con lo que sabes que tienes: cada IP que no
  reconozcas es un dispositivo a identificar (cámara, TV, portátil del
  vecino si tu wifi está abierta...)
- `host.X` conocido con MAC distinta = posible suplantación
- El barrido ping no ve hosts que bloquean ICMP: nmap es más fiable

**Tiempo**: nmap ~10-20 s; barrido ping ~1-2 min.

---

## w3m-sniff — Captura de paquetes (tcpdump)

**Para qué**: ver el tráfico en vivo de tu red: quién habla con quién,
qué protocolos, errores.

**Uso:**
1. Elige interfaz con el botón (**eth0 → wlan0 → any**, cíclico)
2. Filtro opcional en el campo blanco (sintaxis tcpdump):
   - `host 192.168.1.10` — solo ese host
   - `port 80 or port 443` — solo web
   - `not port 22` — ignora tu SSH
   - `icmp` — solo pings
3. **Start** / **Stop**; **Limpiar** vacía el buffer

**Interpretación:**
- Líneas `IP x.x.x.x.y > z.z.z.z.w: Flags [S]` — [S]=inicio de
  conexión, [F]=cierre, [R]=reseteo brusco (anómalo si abunda)
- Muchos `[S]` sin respuesta = algo intenta conectar sin éxito
- Con filtro `port 53` ves cada consulta DNS: destinos extraños = apps
  phoning home

**Permisos**: tcpdump necesita root (la distro W3M entra como root;
en Arch usa `sudo ./w3m-sniff`).

---

## w3m-traf — Monitor de tráfico

**Para qué**: ver cuánto ancho de banda usa cada interfaz, en vivo,
con historial de 2 minutos.

**Uso**: no tiene botones — se actualiza sola cada segundo leyendo
`/proc/net/dev`.

**Interpretación:**
- **RX** (azul) = descarga; **TX** (gris) = subida; totales acumulados
  a la izquierda, sparkline de tasas a la derecha
- RX constantemente alto sin que estés descargando nada → algo consume
  tu ancho: identifica con w3m-sniff (`not port 443` para descartar
  tráfico web normal)
- Picos periódicos de TX = telemetría/actualizaciones automáticas

---

## w3m-nc — Netcat gráfico

**Para qué**: probar tus servicios a mano: conectarte a un puerto y
hablar el protocolo (HTTP, SMTP, tu propio server).

**Uso:**
1. Clic en el campo host → escribe destino; Tab pasa al campo puerto;
   Tab de nuevo al área de envío
2. **Conectar** (o Enter)
3. Escribe líneas y Enter para enviarlas; la respuesta aparece arriba
4. **Cerrar** corta la conexión

**Ejemplo — probar un servidor web propio:**
```
host: 192.168.1.5    puerto: 8000    Conectar
> GET / HTTP/1.0
> (Enter en línea vacía no envía; usa:)
> [enviar "GET / HTTP/1.0" y luego "Host: x" y una línea vacía]
```

**Interpretación:**
- `connect: Connection refused` → nada escucha ahí (confirma con
  w3m-ports en el servidor)
- Conecta y no responde → el servicio está pero no habla tu protocolo
- `--- cerrado por el remoto ---` → el servidor cortó (normal en HTTP
  tras responder)


---

## w3m-dns — Análisis DNS

**Para qué**: resolver dominios, ver registros (A/AAAA/CNAME/MX/NS/PTR),
hacer reverse DNS, y comparar resolvers.

**Uso:**
1. Escribe el dominio en el campo principal (Tab salta al campo server)
2. Elige tipo: **A** (IPv4), **AAAA** (IPv6), **CNAME** (alias), **MX**
   (mail), **NS** (nameservers), **PTR** (reverse — escribe la IP)
3. **Consultar** (o Enter)
4. Campo server opcional: vacío = resolver del sistema; o prueba
   `8.8.8.8`, `1.1.1.1` para comparar respuestas

**Interpretación:**
- A con varias IPs = balanceo/CDN (normal en grandes servicios)
- Compara el mismo dominio contra tu resolver y 1.1.1.1: respuestas
  distintas pueden indicar cacheo raro o manipulación del router
- PTR de una IP de tu red (de w3m-scan) te da el nombre del dispositivo
- Sin respuesta = dominio sin ese tipo de registro (usa NS para ver
  qué servers tienen autoridad)

**Motor**: `dig` si está, si no `nslookup` (busybox).

---

## w3m-route — Ruteo

**Para qué**: ver la tabla de rutas completa y, lo más útil, saber
**por dónde saldrá un paquete** a una IP concreta antes de enviarlo.

**Uso:**
1. Abre y verás la tabla (rutas default resaltadas en azul)
2. Escribe una IP destino y pulsa **¿Por dónde?** — resalta la ruta
   que aplicará (la más específica que la contiene, o la default)
3. F5 refresca

**Interpretación:**
- `0.0.0.0  192.168.1.1` = default gateway: todo lo desconocido sale
  por ahí. Si no existe esta línea, no tienes internet
- Dos rutas al mismo destino con distinta métrica: gana la menor
- ¿Por dónde? dice "ruta específica" cuando hay una más concreta que
  la default (ej. VPN que captura `10.0.0.0/8`) — clave para entender
  por qué algo no sale por donde esperabas

---

## w3m-arp — Tabla ARP

**Para qué**: ver qué MAC corresponde a cada IP de tu red y detectar
**conflictos de IP** (dos MACs pidiendo la misma IP).

**Uso:**
1. **Refrescar** — lee la caché ARP actual
2. **Hacer ping a todas** — pinga cada IP de la tabla para repoblar la
   caché (lento, ~1 s por entrada)
3. F5 = refrescar

**Interpretación:**
- Cada línea = un dispositivo real que ha hablado contigo
- Entrada en azul = **conflicto**: misma IP respondió con dos MACs —
  DHCP duplicado típico o suplantación; investiga con w3m-scan
- `incomplete` = la IP no respondió ARP (caída o filtrada)

---

## w3m-link — Estado del enlace

**Para qué**: la capa física: ¿hay portador (cable/reds)?, velocidad,
dúplex, y errores de transmisión acumulados.

**Uso:** solo **Refrescar** (F5). Sin botones, lee sysfs.

**Interpretación:**
- `CARRIER` = enlace físico presente; `no-link` = sin cable o wifi
  desconectado
- `100 Mbps` cuando tu red es gigabit = cable malo/Cat5 antiguo: renegoció
  abajo
- `err: rx=N` creciendo entre refrescos = ruido eléctrico o NIC fallando
  (cambia cable antes de culpar al driver)
- `full` en duplex; `half` indica problema de negociación (hub antiguo)

---

## Requisitos por app

| App | Requiere | Degrada a / avisa |
|---|---|---|
| w3m-ping | busybox ping, traceroute | — |
| w3m-ifaces | busybox ifconfig/ip, udhcpc, iwlist | "iwlist no disponible" |
| w3m-ports | netstat o ss | "no disponibles" |
| w3m-scan | nmap (recomendado) o busybox ping | barrido ping básico |
| w3m-sniff | tcpdump (root) | aviso en barra |
| w3m-traf | /proc/net/dev (siempre) | — |
| w3m-nc | libc (sockets directos) | — |
