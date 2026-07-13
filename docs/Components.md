# Componentes del Sistema

## Proposito

Este documento describe los modulos actuales del servidor HTTP y sus responsabilidades academicas. La regla practica es simple: cada modulo debe explicar una pieza de bajo nivel sin esconderla detras de frameworks.

## server.c

Proposito: inicializar el servidor TCP.

Funciones principales:

* `server`
* `htons`

Dependencias internas:

* `epoll_loop.h`
* `io.h`
* `syscalls.h`

Riesgos:

* puerto fijo `5050`
* no hay configuracion por argumentos
* errores de socket solo se registran como texto generico

Pruebas sugeridas:

```bash
make
./bin/server
curl -v http://localhost:5050/
```

## epoll_loop.c

Proposito: administrar eventos de socket con `epoll`.

Funciones principales:

* `event_loop`
* `get_client`
* `new_client`

Dependencias internas:

* `client.h`
* `requests.h`
* `syscalls.h`

Riesgos:

* manejo incompleto de errores de `epoll`
* no valida `sys_accept` negativo antes de crear cliente
* posible doble cierre de fd en rutas de error
* arreglo estatico de clientes con capacidad fija

Pruebas sugeridas:

```bash
curl -v http://localhost:5050/
curl -v http://localhost:5050/no-existe
```

## client.c

Proposito: representar el estado de una conexion HTTP.

Estructuras importantes:

* `client`
* `string_pool`
* `hash_map`

Funciones principales:

* `client_create`
* `client_destroy`
* `client_reset`

Riesgos:

* `CLIENT_BUF_SIZE` limita headers y body pequenos
* el reset no se usa de forma completa para keep-alive real
* `client_destroy` cierra el fd, por tanto otros modulos no deben cerrarlo otra vez
* `client.query` (parametros de query string, ver `query.c`) duplica el costo fijo de un `hash_map` completo por cliente, ademas del de `headers`

## requests.c

Proposito: parsear HTTP y ejecutar handlers.

Funciones principales:

* `read_incoming`
* `write_response`
* `write_headers`
* `get`
* `post`

Riesgos:

* modulo demasiado grande
* parser de headers no valida entradas malformadas
* parser JSON de `"key"` es manual
* body se carga completo en memoria
* no hay limite HTTP formal para payload

Ejemplos:

```bash
curl -v http://localhost:5050/index.html
curl -v -X POST http://localhost:5050/database -d '{"key":"uno","v":1}'
```

## query.c

Proposito: separar el query string del path en el punto de parseo (`write_response`, antes de `route_request`) y exponerlo como `hash_map` en `client.query`, con el mismo mecanismo que ya usa `client.headers`.

Funciones principales:

* `query_split` — corta `raw` en el primer `?` (in-place) y devuelve el resto, o `0` si no hay `?`.
* `query_parse` — separa por `&`, cada segmento por el primer `=`, decodifica percent-encoding y `+`, y guarda cada par en el `hash_map` recibido usando `pool` para las copias.

Dependencias internas:

* `hashmap.h`
* `str.h`

Riesgos:

* claves/valores decodificados se acotan a un buffer de pila de 256 bytes (`QUERY_TOKEN_CAP`); valores mas largos se truncan silenciosamente
* `%` seguido de menos de 2 hex digits validos se copia literal en vez de fallar — tolerante, no estricto
* segmentos vacios (`&&`, `&` final) se ignoran sin aviso

Ejemplos:

```bash
curl -v "http://localhost:5050/index.html?x=1"
curl -v "http://localhost:5050/database/smoke?debug=1&trace"
```

## database.c

Proposito: adaptar HTTP a `microdb`.

Funciones principales:

* `database_set`
* `database_route`
* `database_list`

Dependencias internas:

* `json.h`
* `requests.h`
* `microdb/src/include/db.h`

Riesgos:

* usa ruta fija `./data.wal` (centralizada en `database_open` local)
* el valor JSON se reinyecta como objeto sin validar que sea JSON valido
* `database_list` carga en memoria el valor completo de cada registro del namespace (ver `docs/MicroDB-Integration.md`)

## json.c

Proposito: construir respuestas JSON simples.

Funciones principales:

* `json_object_init`
* `json_add_string`
* `json_add_number`
* `json_add_bool`
* `json_add_object`
* `json_serialize`
* `json_array_init`
* `json_array_add_object`
* `json_array_serialize`

Riesgos:

* no es parser JSON completo
* limite fijo `JSON_MAX_FIELDS`
* `json_add_object` inserta texto crudo
* `json_array` no es un tipo de campo dentro de `json_object`: se serializa aparte y se incrusta con `json_add_object`, quien llama debe mantener vivo el `string_pool` del array hasta serializar

## str.c

Proposito: utilidades de string y arena `string_pool`.

Funciones principales:

* `len`
* `strcmp`
* `strncmp`
* `substr`
* `string_pool_alloc`
* `string_pool_append`
* `string_pool_format`

Riesgos:

* varias funciones no validan punteros nulos
* `string_pool_format` soporta pocos formatos
* `substr` no termina con `\0`

## io.c

Proposito: entrada/salida y formateo minimo.

Funciones principales:

* `read`
* `write`
* `writef`
* `logf`
* `readline_stream`
* `sendfile`

Riesgos:

* `format` no cubre todos los formatos C
* `readline_stream` mezcla lectura bloqueante/no bloqueante
* lectura de lineas largas falla con `-2`
* `open`/`close`/`read`/`write` tienen visibilidad global y coinciden en nombre con libc: sin `-fvisibility=hidden` en el `Makefile` (ya presente), interponen sobre las llamadas equivalentes dentro de `libmicrodb.so` — ver `docs/Architecture.md` ("Riesgo Critico: Interposicion de Simbolos con microdb"). No renombrar estas funciones ni quitar la flag sin entender ese riesgo.

## memory.c

Proposito: asignacion por `mmap` y liberacion por `munmap`.

Funciones principales:

* `sysmap_alloc`
* `sysmap_free`

Riesgos:

* no hay realloc general
* no hay tracking global de leaks
* cada reserva se alinea a pagina, lo cual simplifica pero desperdicia memoria

## syscalls.c

Proposito: centralizar llamadas al sistema Linux x86_64.

Funciones principales:

* `sys_call0` a `sys_call5`
* `sys_read`
* `sys_write`
* `sys_socket`
* `sys_epoll_wait`
* `sys_sendfile`

Riesgos:

* no existe helper generico de 6 argumentos
* el proyecto depende de ABI Linux x86_64
* errores retornan negativos y cada caller debe interpretarlos

## Compatibilidad con Obsidian

Los documentos usan Markdown plano, tablas simples y bloques de codigo. No dependen de extensiones externas salvo diagramas Mermaid si el visor los soporta.

