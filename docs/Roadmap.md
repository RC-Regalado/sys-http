# Roadmap

## Criterio

El proyecto prioriza aprendizaje tecnico, correctitud HTTP, estabilidad y documentacion. Las mejoras deben mantener el estilo educativo: codigo visible, dependencias minimas y cambios pequenos.

## Fase 1 - Corregir Fallos Internos

Objetivo: eliminar errores que pueden causar perdida de datos o comportamiento HTTP incorrecto.

Tareas:

* ~~llamar `db_close` en `database_set` y `database_route`~~ Hecho.
* ~~corregir interposicion de simbolos `open`/`close`/`read`/`write` entre `bin/server` y `libmicrodb.so`~~ Hecho: `-fvisibility=hidden` en el `Makefile`. Ver `docs/Architecture.md` ("Riesgo Critico: Interposicion de Simbolos"). Este bug rompia `db_open` (y por tanto todo `POST`/`GET /database*`) en cualquier base de datos nueva sin `base.db` previo.
* ~~evitar que listar/contar por namespace cargue cada blob completo a memoria~~ Hecho: `db_selector_t.skip_blob` en `microdb`. Ver `docs/MicroDB-Integration.md`.
* validar `:` en `parse_headers`
* corregir bloqueo de path traversal para segmentos `..`
* normalizar `write_headers` para que todos los status tengan el mismo contrato
* evitar doble cierre de fd en rutas de error
* hacer `string_n_copy` seguro con terminador nulo

Pruebas:

```bash
make
curl -v http://localhost:5050/../etc/passwd
curl -v -X POST http://localhost:5050/database -d '{"key":"uno"}'
curl -v http://localhost:5050/database/uno
```

## Fase 2 - Separar HTTP Parser, Router y Handlers

Objetivo: reducir el tamano y riesgo de `requests.c`.

Tareas:

* mover parsing de request line y headers a un modulo dedicado
* crear router minimo para `GET` y `POST`
* mover archivos estaticos a handler propio
* mover database HTTP a handler propio
* mantener funciones publicas actuales hasta documentar impacto

Resultado esperado:

```txt
requests.c  -> parser y modelo HTTP
handlers.c  -> dispatch por ruta/metodo
files.c     -> archivos estaticos y streaming
database.c  -> adaptador microdb
```

## Fase 3 - HTTP/1.1 Basico Correcto

Objetivo: responder correctamente a casos comunes.

Tareas:

* validar version HTTP
* responder `405 Method Not Allowed`
* responder `411 Length Required` cuando POST requiere body
* responder `413 Payload Too Large` para body fuera de limite
* validar `Content-Type` en `/database`
* definir limite de headers y body

## Fase 4 - MicroDB como Store Principal

Objetivo: estabilizar contrato HTTP sobre `microdb`.

Tareas:

* ~~usar namespace explicito en lectura~~ Hecho en `database_route` (`namespace_name = "database"`).
* ~~agregar conector de listado por namespace~~ Hecho: `database_list` + `GET /database/namespace/<nombre>` (base reusable para `files`, `notes`, `media`).
* documentar formato de registros
* agregar funciones para `files`, `notes` y `media`
* crear herramienta de migracion desde sqlite del proyecto `old`
* decidir si blobs grandes usan `DB_RECORD_BLOB` o `DB_RECORD_FILE`

## Fase 5 - Migrar Funciones del Proyecto Old

Objetivo: recuperar capacidades historicas sin portar la arquitectura vieja.

Funciones a migrar:

* `/music`: listar `music` y `video`
* `/file` o `/files/<id>`: servir blobs subidos — diseno ya analizado en `docs/MicroDB-Integration.md` (seccion "Diseno de Operaciones de Archivo"): `POST /files/<nombre>` (crear), `GET /files` (listar, reusa `database_list`), `GET /files/<id>` (descargar). Pendiente de implementar.
* `/notes`: listar, crear y leer notas
* uploads multipart

No migrar directamente:

* dependencia sqlite
* CGI por variables de entorno
* thread pool C++
* downloader con rutas hardcodeadas

## Fase 6 - Pruebas Repetibles

Objetivo: que cada cambio tenga una verificacion pequena.

Tareas:

* scripts curl en `scripts/`
* colecciones para `http_handler` si se agrega la herramienta
* pruebas `make -C microdb test`
* casos de JSON escapado
* casos de rutas malformadas

## Fase 7 - Observabilidad Educativa

Objetivo: facilitar diagnostico sin meter framework.

Tareas:

* logs por conexion
* logs por request
* logs por status HTTP
* logs de errores microdb
* modo debug desde `.project-envrc` si aplica

## Puntos a Mejorar

* rutas y handlers estan demasiado acoplados
* no hay contrato unico para respuestas HTTP
* parsing JSON es parcial
* memoria y ownership deben documentarse por funcion critica
* falta migrador sqlite -> microdb
* falta prueba automatica minima para el servidor
* el README no refleja la integracion actual con `microdb`

