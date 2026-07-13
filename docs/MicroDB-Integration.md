# Integracion con MicroDB

## Proposito

`database.c` es el adaptador entre el servidor HTTP y `microdb`. Su responsabilidad es traducir rutas HTTP a operaciones de base de datos sin exponer detalles internos de almacenamiento al resto del servidor.

## Estado Actual

`microdb` se compila como libreria compartida:

```txt
microdb/bin/libmicrodb.so
```

El `Makefile` principal copia la libreria a:

```txt
bin/libmicrodb.so
```

El servidor enlaza con:

```make
-L./bin -lmicrodb
```

## API Usada

Desde `microdb/src/include/db.h` se usan:

* `db_open`
* `db_write`
* `db_read`
* `db_record_meta_t`
* `db_selector_t`
* `db_record_view_t`

## Escritura

`POST /database` llama:

```c
database_set(key, payload, payload_len)
```

El adaptador escribe:

```txt
wal_path = ./data.wal
namespace = database
content_type = json
flags = DB_RECORD_JSON
key = valor de "key"
value = payload JSON completo
```

## Lectura

`GET /database/<id>` llama:

```c
database_route(client, id)
```

El adaptador construye un `db_selector_t` con:

```txt
selector.key = id
selector.klen = len(id)
```

Despues serializa el resultado como JSON.

## Listado por Namespace

`GET /database/namespace/<nombre>` llama:

```c
database_list(client *cl, const char *namespace_name)
```

El adaptador arma un `db_selector_t` con `namespace_name` fijo y `key = 0`. `db_read` en `microdb/src/db.c` detecta que no hay `key` pero si `namespace_name`, y en ese caso recorre `db->namespace_index` (poblado por `db_index_record` en cada `db_write`) en vez de iterar todo el store. Es decir, `microdb` ya soportaba listar por namespace a nivel de motor; `database_list` solo expone ese camino desde HTTP.

Cada registro encontrado se serializa como `{"id":..., "json":bool, "value":...}` y se acumulan en un arreglo JSON (`json_array`, ver `src/json.c`) que se incrusta en la respuesta final `{"count":N,"namespace":"...","items":[...]}`. A diferencia de `database_route` (busqueda por clave), una lista vacia responde `200`, no `404`.

## Contrato HTTP

Escritura:

```bash
curl -v -X POST http://localhost:5050/database -d '{"key":"uno","v":1}'
```

Lectura:

```bash
curl -v http://localhost:5050/database/uno
```

Listado:

```bash
curl -v http://localhost:5050/database/namespace/database
```

## Migracion desde el Proyecto Old

El proyecto viejo usaba SQLite en `sync.db`:

* tabla logica `files`: `id`, `name`, `hash`, `date`
* tabla logica `notes`: `id`, `name`, `hash`, `date`
* archivos en `sources/uploads/<hash>`
* notas en `sources/notes/<hash>`
* media listada desde `sources/media/music` y `sources/media/video`

Migracion recomendada a `microdb`:

* namespace `files` para metadata de archivos
* namespace `notes` para metadata de notas
* namespace `media` para indices de audio/video
* `DB_RECORD_JSON` para metadata
* `DB_RECORD_BLOB` o `DB_RECORD_FILE` para payload grande

Formato sugerido para metadata:

```json
{
  "id": "id",
  "name": "nombre",
  "hash": "hash_original",
  "date": 0,
  "kind": "file",
  "blob_id": "id_blob"
}
```

## Diseno de Operaciones de Archivo (analisis, no implementado)

Diseno propuesto para Fase 5 del roadmap, apoyado en el mismo conector `database_list` y en el soporte nativo de blobs de `microdb` (`DB_RECORD_BLOB`/`DB_RECORD_FILE`, `db_write_blob_file`/`db_read_blob_file` en `microdb/src/db.c`). No requiere cambios en `microdb/`.

**Modelo de almacenamiento**: un solo registro por archivo, sin separar metadata y blob en dos escrituras. `key = nombre del archivo`, `namespace = "files"`, `content_type = Content-Type del request` (o `application/octet-stream` por defecto), `flags = DB_RECORD_FILE | DB_RECORD_BLOB`. Forzar siempre `DB_RECORD_BLOB` (no depender solo de `inline_threshold`) evita que archivos pequenos queden inline y archivos grandes como blob de forma inconsistente. El nombre de archivo actua como `id` publico; el nombre fisico en `blobs/` lo genera `db_hex_id` (hash interno), por lo que un `id` HTTP arbitrario no representa riesgo de path traversal a nivel de filesystem.

**Crear**: `POST /files/<nombre>` con el body crudo como contenido, igual que `POST /database` ya usa un body crudo sin multipart (Fase 3 del roadmap sigue sin soporte multipart). Respuesta:

```json
{"ok":true,"id":"<nombre>","size":123,"content_type":"..."}
```

**Listar**: `GET /files` reusa `database_list(cl, "files")` (mismo conector de esta seccion), devolviendo `{id, content_type, json, blob, value}` por archivo, con `value:null` para blobs (ver `skip_blob` abajo).

**Descargar**: `GET /files/<id>` usa `db_read` con selector `{key=id, namespace_name="files", skip_blob=0}` (mismo camino que ya usa `database_route`) y escribe el contenido con el `Content-Type` guardado y `Content-Disposition: attachment; filename="<id>"`.

### Riesgo de `db_invoke_record_cb` (resuelto)

`db_invoke_record_cb` (`microdb/src/db.c`) cargaba **siempre** el blob completo a memoria (`db_read_blob_file` con `xmalloc`) antes de invocar el callback, incluso cuando `db_read` se usaba solo para *listar* o *contar* por namespace. Un `GET /files` (o el propio `GET /database/namespace/<n>`) habria cargado en memoria el contenido de **todos** los archivos del namespace solo para reportar su tamano — y el caso ya estaba activo en produccion: `database_add_note` (namespace `notes`) cuenta los registros existentes en cada `POST /database/notes` para generar el siguiente `id`, cargando cada nota completa en memoria solo para incrementar un contador.

**Fix aplicado** (`microdb/src/include/db.h`, `microdb/src/db.c`): se agrego el campo `int skip_blob` a `db_selector_t`. Si es distinto de cero, `db_invoke_record_cb` deja `view.data = NULL, view.dlen = 0` para registros `DB_RECORD_BLOB` en vez de leer el archivo de `blobs/`; los registros inline (no-blob) siguen exponiendo su valor sin costo extra, porque ya estan en memoria. Los tres callers de `db_invoke_record_cb` (`db_iterate_primary_cb`, la ruta de clave exacta en `db_read`, `db_delete_where`) propagan `selector->skip_blob`; el camino de escritura (`db_write`) sigue invocando el callback con `skip_blob=0` siempre (comportamiento sin cambios).

Todo `db_selector_t` existente que se construye con `memset(&selector, 0, sizeof selector)` (todo `microdb/src/main.c`, `db_get`/`db_del` en `db.c`) preserva el comportamiento anterior sin cambios (`skip_blob=0` por defecto). Los selectores de `src/database.c` que **no** usan memset (inicializacion campo por campo) fijan `skip_blob` explicitamente: `0` en `database_route` (busqueda exacta, necesita el valor completo), `1` en `database_list` (listar no debe cargar cada blob) y en `database_add_note` (contar no usa `record->data`).

Se agrego el comando `COUNT <namespace>` al REPL de `microdb` (`microdb/src/main.c`) usando `skip_blob=1`, y un caso de prueba en `microdb/tests/metadata.sh` que escribe un blob y verifica `COUNT` sobre su namespace sin fallar ni depender de cargar el contenido.

## Fallos Internos

* ~~`database_set` no llama `db_close`.~~ Corregido: `database_set` y `database_route` cierran `db_t` en todo camino de salida.
* ~~`database_route` no filtra por namespace `database`; si otra clave existe en otro namespace podria cruzarse.~~ Corregido: `database_route` fija `selector.namespace_name = "database"`.
* `database_set` retorna directamente `db_write`, pero no normaliza errores HTTP.
* `load_record` usa `json_add_object` para insertar `record->data` sin validar que sea JSON.
* la ruta `./data.wal` esta fija y depende del directorio de ejecucion.
* no hay bloqueo ni serializacion explicita de escrituras concurrentes.
* ~~`database_list`/`database_add_note` cargaban cada blob completo a memoria solo para listar o contar.~~ Corregido con `db_selector_t.skip_blob` (ver seccion arriba).

## Mejoras Recomendadas

1. ~~Cerrar siempre `db_t` con `db_close`.~~ Hecho.
2. ~~Fijar namespace en `db_selector_t`.~~ Hecho en `database_route`.
3. ~~Agregar una funcion `database_open` local para centralizar ruta y errores.~~ Hecho (`static database_open` en `src/database.c`).
4. Separar handlers HTTP de operaciones puras de DB.
5. Crear un migrador `sqlite -> microdb` como herramienta externa, no dentro del servidor.
6. Documentar formato estable de registros `files`, `notes` y `media` (parcialmente cubierto arriba para `files`).

## Pruebas de MicroDB

```bash
make -C microdb
make -C microdb test
make
curl -v -X POST http://localhost:5050/database -d '{"key":"uno","v":1}'
curl -v http://localhost:5050/database/uno
curl -v http://localhost:5050/database/namespace/database
```

