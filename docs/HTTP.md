# Comportamiento HTTP

## Proposito

Este documento registra el contrato HTTP actual del servidor. No describe HTTP/1.1 completo; describe lo que el proyecto implementa hoy.

## Servidor

* Puerto por defecto: `5050`
* Protocolo: HTTP sobre TCP
* Conexion: se fuerza `Connection: close`
* Raiz estatica: `templates/`

## GET estatico

Request:

```http
GET /index.html HTTP/1.1
Host: localhost
```

Flujo:

* `/` se traduce a `/index.html`
* la ruta se copia a `client.path`
* se antepone `templates/`
* se abre el archivo
* se responde con `Content-Type`, `Content-Length` y `sendfile`

Codigos actuales:

* `200 OK` si existe
* `404 Not Found` si no existe
* `500 Internal Server Error` si falla `stat`

Riesgo actual:

* la proteccion contra `..` es insuficiente porque solo compara la ruta completa con `..`

## GET video

Si la extension es `.mp4`, el servidor responde:

```http
HTTP/1.1 200 OK
Content-Type: video/mp4
Transfer-Encoding: chunked
Connection: close
```

El cuerpo se envia en fragmentos de 256 bytes.

Riesgos:

* no soporta `Range`
* no soporta pausa/reanudacion
* no valida backpressure real
* el MIME esta fijo a `video/mp4`

## GET base de datos

Ruta:

```http
GET /database/<id> HTTP/1.1
```

Comportamiento:

* busca `<id>` en `microdb`
* retorna JSON si encuentra registro
* retorna `404` si no encuentra registro

Ejemplo:

```bash
curl -v http://localhost:5050/database/uno
```

Respuesta esperada si existe:

```json
{"found":true,"value":{"key":"uno"},"json":true,"id":"uno"}
```

## GET listado por namespace

Ruta:

```http
GET /database/namespace/<nombre> HTTP/1.1
```

Comportamiento:

* usa el conector `database_list` (`src/database.c`), que arma un `db_selector_t` solo con `namespace_name` (sin `key`)
* `microdb` recorre su `namespace_index` interno y devuelve todos los registros de ese namespace
* siempre responde `200`, incluso si el namespace no tiene registros (una lista vacia no es error)
* no expone el body crudo del registro sin decodificar; cada item incluye `id`, `json` y `value`

Ejemplo con datos:

```bash
curl -v http://localhost:5050/database/namespace/database
```

```json
{"count":2,"namespace":"database","items":[{"id":"uno","json":true,"value":{"key":"uno","v":1}},{"id":"dos","json":true,"value":{"key":"dos","v":2}}]}
```

Ejemplo sin datos:

```bash
curl -v http://localhost:5050/database/namespace/no-existe
```

```json
{"count":0,"namespace":"no-existe","items":[]}
```

Si el segmento `<nombre>` viene vacio (`GET /database/namespace/`), responde `404`.

## POST base de datos

Ruta:

```http
POST /database HTTP/1.1
Content-Type: application/json
Content-Length: ...

{"key":"uno","valor":123}
```

Comportamiento:

* lee el body usando `Content-Length`
* extrae el campo `"key"`
* guarda el payload JSON completo como valor
* usa namespace `database`
* marca el registro con `DB_RECORD_JSON`

Respuesta si guarda:

```json
{"ok":true,"key":"uno"}
```

Respuesta si falta `"key"`:

```json
{"ok":false,"key":null}
```

Limitaciones:

* no valida `Content-Type`
* no parsea JSON completo
* `"key"` debe ser string
* maximo efectivo depende de `string_pool` y memoria disponible

## Headers

Headers de request soportados de forma practica:

* `Content-Length`
* `Connection`
* `Accept`

Headers de respuesta usados:

* `Content-Type`
* `Content-Length`
* `Transfer-Encoding`
* `Connection`

## Fallos Internos

* `write_headers(OK)` no termina la seccion de headers; los callers deben agregar el resto.
* `write_headers` para errores agrega `\r\n\r\n`, lo cual hace inconsistente la API.
* metodos distintos de `GET` y `POST` no retornan `405`.
* no se valida version HTTP.
* no se validan headers duplicados.
* no se soporta query string.
* no hay limite formal de tamano para headers.

## Pruebas Manuales

```bash
curl -v http://localhost:5050/
curl -v http://localhost:5050/index.html
curl -v http://localhost:5050/no-existe
curl -v http://localhost:5050/../etc/passwd
curl -v -X POST http://localhost:5050/database -d '{"key":"uno","v":1}'
curl -v http://localhost:5050/database/uno
curl -v http://localhost:5050/database/namespace/database
```

