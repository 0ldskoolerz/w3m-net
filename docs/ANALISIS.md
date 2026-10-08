# w3m-net — Guía de análisis de red completo

Metodología para auditar **tu propia red** combinando las 7 aplicaciones.
Cada fase produce datos que la siguiente consume. Sigue el orden.

---

## Mapa general: 5 fases

```
FASE 1: Estado local        w3m-ifaces → w3m-ports → w3m-link
FASE 2: Inventario LAN      w3m-scan → w3m-arp → w3m-dns (PTR)
FASE 3: Salud de enlaces    w3m-ping → w3m-route → w3m-traf
FASE 4: Inspección profunda w3m-sniff → w3m-nc
FASE 5: Diagnóstico final   síntesis + acción
```

---

## FASE 1 — ¿Cómo estoy yo? (5 min)

**w3m-ifaces**: confirma que tienes IP válida (DHCP si hace falta) y
anota: tu IP, el gateway (aparece en la ruta), tu interfaz activa.

**w3m-link**: estado físico — si el gateway pierde paquetes en la
Fase 3, aquí verificas si es físico: `no-link`, `100 Mbps` en red
gigabit (cable viejo), o errores rx creciendo (cable/cableado dañado).
Es el paso "descarta lo físico antes de lo lógico".

**w3m-ports**: haz inventario de tus propias puertas:
- Lista cada LISTEN y su proceso: ¿todos son tuyos y esperados?
- Cualquier `0.0.0.0:X` que no reconozcas → apúntalo para la Fase 4

> **Resultado de fase**: tu IP, gateway, lista de servicios propios.

## FASE 2 — ¿Quién más hay en mi red? (2-20 min)

**w3m-scan → Escanear LAN**: inventario de la /24.

**w3m-arp** tras el escaneo: cruza la caché ARP con los resultados del
scan — cada dispositivo encontrado debe tener MAC coherente; un
**conflicto de IP** (azul) resuelto aquí explica comportamientos raros
tipo "la impresora funciona a veces".

**w3m-dns** con tipo **PTR** para las IPs descubiertas: el reverse DNS
suele nombrar el dispositivo (`raspberrypi.local`, `android-xx...`) —
la forma más rápida de identificar desconocidos.

Cruza los resultados:
- Dispositivos esperados (router, tus máquinas) → confirma sus IPs
- **Desconocidos**: identifica cada uno. En w3m-sniff filtra
  `host <IP sospechosa>` unos 30 s: ¿con qué habla? ¿Puertos conocidos?
- MACs duplicadas para la misma IP → investigación inmediata

> **Resultado**: mapa IP↔dispositivo de tu red, lista de sospechosos.

## FASE 3 — ¿La red funciona bien? (5 min)

**w3m-route** primero: confirma que existe default gateway y comprueba
con **¿Por dónde?** que una IP externa sale por la ruta esperada (sin
VPN inesperada capturando tráfico).

**w3m-ping** sobre 3 objetivos en este orden:
1. **Gateway** (`192.168.1.1` típico) — si pierde paquetes, el problema
   es local (wifi/cable) y todo lo demás heredará latencia
2. **Un host de tu LAN** (de la Fase 2) — salud del segmento interno
3. **8.8.8.8 y un dominio** (ej. `dns.google`) — si IP va bien pero el
   dominio no, tu problema es DNS, no la red

**w3m-traf** déjalo abierto 2 minutos: tasas base RX/TX y detectas si
algo consume ancho estando la máquina "tranquila".

> **Resultado**: latencias base, pérdida, consumo normal.

## FASE 4 — Los sospechosos, uno a uno

**w3m-sniff** con filtros progresivos (30-60 s cada uno):
```
host <IP sospechosa>          ¿con quién habla?
port 53                       ¿qué dominios consulta tu red?
not port 443 and not port 80  ¿qué escapa del tráfico web normal?
```
Señales de alerta: muchos `[S]` sin respuesta, conexiones a IPs
extrañas, DNS hacia servidores que no configuraste.

**w3m-nc** para hablar directamente con un servicio descubierto:
- Puerto abierto en un dispositivo de la Fase 2 → conéctate y mira el
  banner de bienvenida (muchos servicios se identifican solos)
- Servicio propio que no responde → prueba `GET / HTTP/1.0` a mano
  para ver si responde el protocolo o está colgado

> **Resultado**: qué es cada desconocido y qué hace.

## FASE 5 — Síntesis y acción

Con los datos de todas las fases, el informe típico responde:

| Pregunta | Fase que la responde |
|---|---|
| ¿Qué dispositivos hay y quiénes son? | 2 |
| ¿Hay algo que no debiera? | 2 + 4 |
| ¿La conexión es sana? | 3 (gateway→LAN→internet) |
| ¿Qué consume mi ancho de banda? | 3 + 4 (`w3m-traf` + filtros sniff) |
| ¿Por qué no llega a un destino concreto? | 3 (`w3m-route` ¿Por dónde?) |
| ¿Quién es cada dispositivo? | 2 (`w3m-dns` PTR + `w3m-arp`) |
| ¿Fallo físico o lógico? | 1 (`w3m-link` errores/carrier) |
| ¿Mis servicios están expuestos? | 1 (LISTEN en 0.0.0.0) |

**Acciones correctivas típicas**: cerrar puertos (firewall), quitar
dispositivos desconocidos (cambiar clave wifi), reubicar el equipo
(RTT alto al gateway = señal wifi débil).

---

## Ejemplo real, paso a paso: "internet va lento"

1. **w3m-traf** abierto → RX saturado sin que hagas nada ✔
2. **w3m-sniff** `not port 443` → un host interno habla sin parar con
   una IP externa por un puerto raro
3. **w3m-scan** → esa IP interna es la cámara IP del salón
4. **w3m-nc** a la cámara:23 → banner de un firmware antiguo
5. **Acción**: firmware de la cámara desactualizado transmitiendo a un
   cloud chino — se actualiza o se aísla en la red de invitados

Todo el análisis: 10 minutos, 4 ventanas, 0 comandos memorizados.

---

## Límites y ética

- Escanea y captura **solo redes donde tienes autorización** (tu casa,
  tu oficina con permiso). Lo contrario es ilegal en la mayoría de
  países.
- w3m-scan/sniff ven tráfico no cifrado; el cifrado (HTTPS) hace que
  solo veas metadatos (quién↔quién, cuánto), no contenido — correcto.
- Para redes con cables/wifi administradas por otros, pide permiso
  por escrito antes de la Fase 4.
