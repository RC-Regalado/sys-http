# Arquitectura del Proyecto

## Resumen

`sys-http` es un servidor HTTP educativo escrito en C y ensamblador para Linux x86_64. Su finalidad academica es exponer el funcionamiento de un servidor web desde bajo nivel: sockets, `epoll`, syscalls, parsing HTTP, memoria manual, estructuras de datos propias e integracion con una base de datos tambien educativa (`microdb`).

El proyecto evita frameworks y mantiene una implementacion explicita. Esta decision reduce comodidad, pero permite estudiar cada capa tecnica.

## Alcance Actual

El servidor soporta:

* inicializacion de socket TCP en el puerto `5050`
* multiplexacion con `epoll`
* lectura basica de peticiones HTTP
* headers en un hash map interno
* lectura de body por `Content-Length`
* `GET` de archivos estaticos desde `templates/`
* envio de archivos por `sendfile`
* streaming chunked para `.mp4`
* `GET /database/<id>` mediante `microdb`
* `GET /database/namespace/<nombre>` lista todos los registros de un namespace
* `POST /database` con JSON minimo y campo obligatorio `"key"`

No soporta todavia:

* HTTP/1.1 completo
* keep-alive real
* routing formal
* multipart uploads
* validacion JSON completa
* concurrencia robusta sobre escritura de base de datos

Query string ya soportado (`query.c`, ver `docs/HTTP.md`): `cl->query` expone los pares clave/valor decodificados; ningun handler los consume todavia, pero el path que llega a los handlers ya no se rompe con `?` en la URL.

## Flujo General

```txt
cliente HTTP
  -> server.c
  -> epoll_loop.c
  -> client.c
  -> requests.c
  -> get/post
  -> files/templates o database.c
  -> io.c/syscalls.c
```

## Capas

### Arranque

`server.c` crea el socket, configura `SO_REUSEADDR`, hace `bind`, ejecuta `listen` e inicia `event_loop`.

### Eventos

`epoll_loop.c` registra el socket servidor y los clientes. Para cada cliente invoca `read_incoming` y luego `write_response`.

### Cliente

`client.c` reserva una estructura `client` con `sysmap_alloc`, inicializa su `string_pool` y su `hash_map`, y configura el fd como no bloqueante.

### HTTP

`requests.c` concentra parsing y despacho. Actualmente interpreta la request line, headers, body, `GET`, `POST`, respuestas de archivo, video chunked y rutas hacia base de datos.

Este modulo esta sobrecargado y es el principal candidato a refactor.

### Archivos

Los archivos estaticos se sirven desde `templates/`. La respuesta normal usa `Content-Length` y `sendfile`; para `.mp4` se usa `Transfer-Encoding: chunked`.

### Base de Datos

`database.c` adapta HTTP a `microdb`. Abre `./data.wal` mediante un `database_open` local, escribe registros JSON en namespace `database`, lee registros por clave y lista registros por namespace (`database_list`). Cierra siempre `db_t` con `db_close`.

`microdb` se compila como libreria compartida `libmicrodb.so` y expone una API en `microdb/src/include/db.h`.

## Modelo de Datos Actual

`POST /database` espera:

```json
{"key":"id","campo":"valor"}
```

El servidor guarda:

```txt
key = id
value = payload JSON completo
namespace = database
content_type = json
flags = DB_RECORD_JSON
```

`GET /database/id` intenta recuperar ese registro y responde JSON.

## Restricciones Tecnicas

* Linux x86_64.
* Uso directo de syscalls.
* Compilacion con `-ffreestanding`, `-nostdlib`, `-nodefaultlibs`.
* No asumir runtime estandar completo.
* No agregar librerias externas.
* Mantener el valor educativo del codigo.

## Riesgo Critico: Interposicion de Simbolos con microdb (Resuelto)

`src/io.c` define, con visibilidad global por defecto, funciones llamadas igual que funciones de libc: `open`, `close`, `read`, `write` (wrappers delgados sobre `sys_open`/`sys_close`/etc.). `bin/server` se enlaza dinamicamente contra `libmicrodb.so` (que a su vez depende de `libc.so.6` normal), y por reglas estandar de resolucion de simbolos ELF, los simbolos globales del ejecutable principal tienen prioridad sobre los de las bibliotecas de las que depende. Esto significa que **toda llamada a `open`/`close`/`read`/`write` hecha desde dentro de `libmicrodb.so` terminaba interceptada por las implementaciones crudas de `src/io.c`**, no por las de glibc.

El sintoma: la version cruda de `open` (`sys_open`) devuelve el convenio de syscall de Linux (negativo = `-errno`, ej. `-2` para `ENOENT`), mientras que microdb esperaba el convenio POSIX de glibc (`-1` + variable `errno`). El chequeo `if (errno == ENOENT) return 0;` en `mmap_load_into_ht` (`microdb/src/mmap.c`) nunca se cumplia, así que `db_open` fallaba (`return -6`) en **cualquier base de datos realmente nueva** (sin `base.db` previo) — es decir, cualquier `POST` a `/database` o `/database/notes` fallaba con `500` la primera vez que se ejecutaba el servidor en un directorio limpio. El bug estaba enmascarado en desarrollo porque `data.wal`/`base.db` ya existian de sesiones previas.

Ademas de `mmap_load_into_ht`, esta interposicion afectaba potencialmente **cualquier** operacion de archivo interna de microdb (WAL, blobs, paginas), ya que todas pasan por `open`/`read`/`write`/`close`. Las firmas de las versiones de `src/io.c` tampoco coinciden con POSIX (ej. `read(long, char*, unsigned short)` limita la longitud a 16 bits; `close(int)` no retorna valor, mientras que microdb a veces revisa `close(fd) != 0`), lo cual podia producir fallas o truncamientos silenciosos adicionales.

**Fix aplicado**: se agrego `-fvisibility=hidden` a `FLAGS` en el `Makefile` raiz. Esto oculta del *dynamic symbol table* de `bin/server` los simbolos que no necesitan ser vistos fuera del propio ejecutable (incluyendo `open`/`close`/`read`/`write`), eliminando la interposicion. Verificado con `nm -D bin/server` (ya no lista esos simbolos) y con pruebas end-to-end sobre una base de datos recien creada (`rm -f data.wal base.db && rm -rf blobs` antes de arrancar el servidor).

## Riesgo Critico: SIGSEGV en `read_incoming` con requests de navegador real (Resuelto)

Sintoma reportado: `bin/server` bajo depurador crasheaba con `SIGSEGV` dentro de `read_incoming`, y en ejecucion normal una request real de navegador (muchas cabeceras: cookies, `User-Agent`, `Accept-*`, `Sec-Fetch-*`) dejaba la pagina sin cargar, mientras que `curl` (pocas cabeceras, request chica) siempre funcionaba. La diferencia no era la direccion (`localhost` vs `127.0.0.1` resuelven ambas al mismo socket dual-stack via `listen_ipv6` con `IPV6_V6ONLY=0`, confirmado con `ss -6ltn`/`ss -4ltn`) sino el tamano/segmentacion de la request — eran dos bugs independientes que una request grande dispara con mucha mas probabilidad que una chica:

1. **Overflow en `readline_stream` (`src/io.c`)**: el primer `read()` de cada vuelta del loop pedia siempre `chunk_len` (1024, fijo desde el unico call site) bytes, sin acotarlo al espacio real que quedaba en `reader->buffer` (tambien 1024 bytes, `LINE_BUF_SIZE`). Si `write_pos > 0` (la request llego en mas de un `read()`, tipico con cabeceras grandes) y el kernel tenia datos suficientes en el socket, escribia mas alla del final del arreglo — stack buffer overflow sobre `line_reader`, sin canario porque el proyecto compila con `-fno-stack-protector`. **Fix**: acotar la longitud pedida a `min(chunk_len, LINE_BUF_SIZE - write_pos)`.
2. **NULL deref en `parse_header_line` (`src/requests.c`)**: `cl->pool` (`CLIENT_BUF_SIZE`, antes 1024 bytes) se agotaba con el volumen real de cabeceras de un navegador; `string_pool_nalloc` devolvia `NULL` y el chequeo existente (`if ((void *)key < NULL)`) nunca detecta esto (comparar un puntero contra "menor que NULL" es siempre falso). El codigo seguia y llamaba `substr(data, key, ...)` con `key == NULL`, escribiendo en la direccion `0` (coincide con el `segfault at 0` visto en el log del kernel). Ademas, `substr` se llamaba de forma redundante despues de que `string_pool_nalloc` ya habia copiado el dato de forma acotada — se elimino esa segunda copia. **Fix**: chequeo `if (!key || !value) return;`, mas validacion temprana de que exista `:` en la linea (ver siguiente seccion), mas subir `CLIENT_BUF_SIZE` a 8192 (similar al limite por defecto de nginx/apache para cabeceras) para que un navegador real no agote el pool en el camino normal.

Verificado reproduciendo una request de ~1.5KB con cabeceras tipo navegador (incluye cookie larga) contra `127.0.0.1` y `localhost` (IPv6 `::1`): antes del fix, `coredumpctl` mostraba `SIGSEGV` en `substr <- parse_header_line <- read_incoming` (`key`/`value` en `0x0`); despues del fix, ambas devuelven `200 OK` sin crashear. Suite completa (`make -C microdb test`, `scripts/http_smoke.sh`) sigue en verde.

## Riesgo Critico: Descargas truncadas/corruptas por objetos `.o` desactualizados (Resuelto)

Sintoma reportado: al cargar el sitio en Firefox, archivos estaticos grandes (ej. `index-B_lim1yr.css`, 236825 bytes) llegaban truncados (en un caso, solo ~113KB de ~231KB); en otras corridas Firefox reportaba un `Content-Length` que en realidad correspondia al tamano de un archivo estatico *distinto* (`_plugin-vue_export-helper-D-5_7kjF.js`, 54837 bytes) para una request a la URL del CSS. `curl` con requests simples no siempre lo disparaba, lo cual hacia parecer que el problema era especifico de Firefox o de conexiones lentas.

Diagnostico: instrumentando `serve_static_file`/`send_static_pending` (`src/files.c`) con `logf` temporales se confirmo que, en las corridas que fallaban, `cl->path` llegaba vacio para `GET /` (servia el directorio `templates/` en vez de `templates/index.html`, y `sendfile()` sobre un fd de directorio devuelve `EINVAL`), y en otras corridas el archivo/tamano servido no coincidia con la URL pedida. El mismo binario, con el mismo codigo fuente, pasaba de fallar de forma repetible a funcionar de forma repetible (40/40 requests `GET /` correctas) tras un `make clean && make`. Esto descarta un bug de logica en el parser HTTP o en el loop de envio de archivos, y apunta a una causa mucho mas basica: **el `Makefile` raiz no generaba dependencias de headers** (`build/%.o: src/%.c` compilaba sin `-MMD -MP`, a diferencia del `Makefile` de `microdb`, que si lo hace). Al editar un header compartido (`client.h`, `files.h`, etc.) sin correr `make clean`, un `make` incremental solo recompilaba los `.c` tocados directamente, dejando otros `.o` ya compilados contra una version *anterior* del mismo `struct` — un desajuste de ABI entre unidades de traduccion (offsets de campos como `cl->path`, `cl->file_remaining`, etc. no coincidiendo entre quien escribe el campo y quien lo lee), que se manifiesta como corrupcion de memoria intermitente y dificil de razonar (no es un crash limpio, son valores incorrectos que dependen de que combinacion de `.o` quedo stale).

**Fix aplicado**: se agrego `-MMD -MP` a `FLAGS` en el `Makefile` raiz (mismo patron ya usado en `microdb/Makefile`), mas `DEP_FILES = $(FILES:.o=.d)` y `-include $(DEP_FILES)` al final del archivo, mas `rm -rf ${DEP_FILES}` en el target `clean`. Con esto, `make` genera un `build/%.d` por cada `.o` listando sus headers, y un `make` incremental si recompila correctamente cualquier `.o` cuyo header cambio. Verificado: `touch src/includes/client.h && make` recompila los 6 `.o` que lo incluyen (antes, sin el fix, un `make` incremental no lo garantizaba).

De paso, se endurecio `send_static_pending` (`src/files.c`): el loop de `sendfile()` trataba **cualquier** error inesperado (no `EAGAIN`/`EWOULDBLOCK`/`EINTR`) igual que una transferencia completa exitosa, cerrando la conexion sin registrar nada aunque `file_remaining > 0` — es decir, un error real de `sendfile` (ej. fd invalido, IO error) producia el mismo sintoma de "descarga truncada sin rastro en el log" que el bug de arriba. Ahora se loguea el error antes de cortar la conexion.

**Postmortem — falso positivo posterior (`NS_ERROR_NET_RESET`)**: despues de aplicar el fix de arriba, el mismo sintoma parecio reaparecer en Firefox (`NS_ERROR_NET_RESET`, descarga del CSS cortada a ~98KB de 231KB) incluso con `make clean && make` ya corrido. Se investigo a fondo sin exito de reproducirlo con `curl`, con Firefox headless (Xvfb) y con Firefox real bajo Xvfb (todas las corridas completaron los 236825 bytes sin error, con y sin cabeceras fragmentadas, con 80 conexiones concurrentes y abortos de cliente simulados). La pista definitiva: **el bug no se reproducia en un perfil de Firefox limpio ni en modo incognito**, solo en el perfil principal del usuario — es decir, no era un bug del servidor sino **una entrada de cache de disco de Firefox corrupta/parcial**, generada por el propio perfil del usuario durante las pruebas manuales hechas *mientras el servidor todavia tenia el bug de objetos `.o` desactualizados* (arriba). Confirmado: borrar la cache de Firefox en el perfil principal resolvio el error sin ningun cambio adicional de codigo.

Leccion para pruebas futuras: si se reproduce un bug de contenido incorrecto/truncado en el navegador durante el desarrollo, **hay que asumir que esa URL queda con una entrada de cache corrupta en el perfil usado para probar**, y limpiar la cache de ese perfil (o probar en una ventana privada / perfil nuevo) antes de dar por bueno o malo un fix posterior. Ver tambien `docs/Testing.md`.

Recomendacion operativa: si despues de este fix se siguen viendo sintomas similares (contenido/tamano incorrecto que no cuadra con la request), correr `make clean && make` como primer paso de diagnostico antes de asumir un bug de logica.

## Fallos Internos Relevantes

* `requests.c` mezcla parsing, routing y handlers; esto vuelve fragil cualquier cambio HTTP.
* ~~`parse_headers` no valida que exista `:` antes de calcular longitudes.~~ Corregido (ver "Riesgo Critico: SIGSEGV en `read_incoming`" arriba).
* El bloqueo de path traversal solo compara `file == ".."` y no cubre rutas como `a/../b`.
* `readline_stream` tiene manejo confuso de `EAGAIN`: usa `||` donde conceptualmente debe validar ambos casos con cuidado. (El overflow de longitud de lectura ya se corrigio, ver arriba; esta confusion de `EAGAIN` en el segundo `read` sigue pendiente aunque en la practica es inofensiva porque `EAGAIN == EWOULDBLOCK`.)
* `write_headers` escribe CRLF final en errores, pero no en `OK`; eso obliga a cada caller a conocer ese detalle.
* `post()` extrae `"key"` con busqueda manual, no con parser JSON real.
* `string_n_copy` no garantiza terminador nulo si el origen ocupa todo el buffer.
* `client_destroy` cierra el fd y `epoll_loop.c` tambien puede cerrarlo en algunos caminos de error.
* `Makefile clean` usa `rm` sin `-f`; falla si no existen artefactos.

## Mejoras Prioritarias

1. Separar `requests.c` en parser HTTP, router y handlers.
2. ~~Corregir ownership de `db_t` con `db_close` en todos los caminos.~~ Hecho.
3. Consolidar generacion de respuestas HTTP en una funcion unica.
4. Endurecer validacion de paths antes de tocar archivos.
5. Definir contrato estable para `microdb` desde HTTP.
6. Agregar pruebas minimas con `curl` para GET, POST y errores.

