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

## Fallos Internos Relevantes

* `requests.c` mezcla parsing, routing y handlers; esto vuelve fragil cualquier cambio HTTP.
* `parse_headers` no valida que exista `:` antes de calcular longitudes.
* El bloqueo de path traversal solo compara `file == ".."` y no cubre rutas como `a/../b`.
* `readline_stream` tiene manejo confuso de `EAGAIN`: usa `||` donde conceptualmente debe validar ambos casos con cuidado.
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

