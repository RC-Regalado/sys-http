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

## Contrato HTTP

Escritura:

```bash
curl -v -X POST http://localhost:5050/database -d '{"key":"uno","v":1}'
```

Lectura:

```bash
curl -v http://localhost:5050/database/uno
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

## Fallos Internos

* `database_set` no llama `db_close`.
* `database_route` no llama `db_close`.
* `database_set` retorna directamente `db_write`, pero no normaliza errores HTTP.
* `database_route` no filtra por namespace `database`; si otra clave existe en otro namespace podria cruzarse.
* `load_record` usa `json_add_object` para insertar `record->data` sin validar que sea JSON.
* la ruta `./data.wal` esta fija y depende del directorio de ejecucion.
* no hay bloqueo ni serializacion explicita de escrituras concurrentes.

## Mejoras Recomendadas

1. Cerrar siempre `db_t` con `db_close`.
2. Fijar namespace en `db_selector_t`.
3. Agregar una funcion `database_open` local para centralizar ruta y errores.
4. Separar handlers HTTP de operaciones puras de DB.
5. Crear un migrador `sqlite -> microdb` como herramienta externa, no dentro del servidor.
6. Documentar formato estable de registros `files`, `notes` y `media`.

## Pruebas de MicroDB

```bash
make -C microdb
make -C microdb test
make
curl -v -X POST http://localhost:5050/database -d '{"key":"uno","v":1}'
curl -v http://localhost:5050/database/uno
```

