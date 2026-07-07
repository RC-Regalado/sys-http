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
* `POST /database` con JSON minimo y campo obligatorio `"key"`

No soporta todavia:

* HTTP/1.1 completo
* keep-alive real
* routing formal
* multipart uploads
* query string
* validacion JSON completa
* concurrencia robusta sobre escritura de base de datos

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

`database.c` adapta HTTP a `microdb`. Abre `./data.wal`, escribe registros JSON en namespace `database` y lee registros por clave.

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

## Fallos Internos Relevantes

* `database.c` abre `db_t` pero no llama `db_close`, lo que puede perder sincronizacion, fd y memoria interna.
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
2. Corregir ownership de `db_t` con `db_close` en todos los caminos.
3. Consolidar generacion de respuestas HTTP en una funcion unica.
4. Endurecer validacion de paths antes de tocar archivos.
5. Definir contrato estable para `microdb` desde HTTP.
6. Agregar pruebas minimas con `curl` para GET, POST y errores.

