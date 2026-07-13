# CLAUDE.md

Contexto de proyecto para Claude Code. Léelo completo antes de proponer o aplicar cualquier cambio. Es un resumen operativo de `Agents.md` y `docs/`; en caso de duda, esos documentos son la fuente completa.

## Qué es este proyecto

`sys-http` es un servidor HTTP **educativo** en C (Linux x86_64), pensado para exponer sockets, `epoll`, syscalls crudas, parsing HTTP y manejo manual de memoria — no para llegar a producción. Se integra con `microdb`, una base de datos también educativa compilada como `libmicrodb.so`.

**Prioridad #1: valor educativo y código explícito por encima de comodidad, abstracciones o rendimiento.** Cualquier sugerencia que "esconda" una capa de bajo nivel detrás de una librería o abstracción va en contra del objetivo del proyecto.

## Reglas duras (no negociables)

1. Cero librerías externas. Cero libc si contradice `-ffreestanding`.
2. No romper APIs internas sin documentar el impacto.
3. No tocar `/microdb` sin justificación técnica explícita.
4. No reemplazar código educativo por soluciones "de alto nivel".
5. No borrar módulos sin aprobación del usuario.
6. Documentación en Markdown, siempre actualizada junto con el código (`docs/`).
7. Antes de tocar el parser HTTP: validar con `curl` y, si aplica, con `/http_handler`.

Flags de compilación (`-g -ffreestanding -fno-builtin -nostdlib -nostartfiles -nodefaultlibs -fno-stack-protector -Wall -Werror -O0`): no asumas runtime estándar disponible.

## Flujo de trabajo esperado antes de implementar

1. Releer este archivo y `docs/Architecture.md`.
2. Identificar el módulo afectado (ver tabla abajo) y revisar su doc en `docs/Components.md`.
3. Revisar las APIs internas existentes que toca el cambio.
4. Proponer un plan breve (usar EnterPlanMode si el cambio no es trivial).
5. Aplicar cambios pequeños e incrementales.
6. `make` (y `make -C microdb` si aplica).
7. Probar con `curl` (ver casos mínimos abajo) y con `/http_handler` si el cambio toca HTTP.
8. Actualizar la documentación afectada en `docs/`.

## Mapa de módulos (`src/`)

| Archivo | Responsabilidad | Estado |
|---|---|---|
| `server.c` | Init socket TCP, `bind`/`listen`, arranca `event_loop` | puerto fijo `5050` |
| `epoll_loop.c` | Loop de eventos, `event_loop`, `get_client`, `new_client` | arreglo estático de clientes, manejo incompleto de errores |
| `client.c` | Estado de conexión: `client`, `string_pool`, `hash_map` | `client_destroy` cierra el fd — nadie más debe cerrarlo |
| `requests.c` | Parsing HTTP + routing + handlers (`read_incoming`, `write_response`, `write_headers`, `handle_get`, `handle_post`) | **sobrecargado, candidato #1 a refactor** |
| `query.c` | Separa query string del path (`query_split`, `query_parse`) y llena `client.query` | claves/valores truncados a 256 bytes |
| `database.c` | Adaptador HTTP ↔ microdb (`database_set`, `database_route`, `database_list`, `database_add_note`) | cierra `db_t` siempre; namespace fijo por selector |
| `json.c` | Constructor JSON simple + `json_array` (no parser completo) | límite fijo `JSON_MAX_FIELDS` |
| `str.c` | Utilidades string + arena `string_pool` | varias funciones no validan NULL |
| `io.c` | I/O y formateo mínimo (`readline_stream`, `sendfile`, `writef`) | mezcla lectura bloqueante/no bloqueante |
| `memory.c` | `sysmap_alloc`/`sysmap_free` sobre `mmap` | sin realloc general, sin tracking de leaks |
| `syscalls.c` | Wrappers de syscalls Linux x86_64 (`sys_call0`..`sys_call5`, etc.) | depende de ABI x86_64 |

Detalle completo de cada módulo (funciones, riesgos, ejemplos): `docs/Components.md`.

## Bugs conocidos ya documentados (no los reintroduzcas, y prioriza corregirlos si tocas ese código)

- `requests.c` / `parse_headers`: no valida que exista `:` antes de calcular longitudes de header.
- Protección de path traversal solo compara `file == ".."`, no cubre `a/../b`.
- `readline_stream`: manejo confuso de `EAGAIN` con `||` mal planteado.
- `write_headers`: inconsistente — agrega `\r\n\r\n` en errores pero no en `OK`, cada caller debe saberlo.
- `post()` extrae `"key"` con búsqueda manual de substring, no con parser JSON real.
- `string_n_copy` no garantiza `\0` si el origen llena el buffer completo.
- Posible doble cierre de fd entre `client_destroy` y `epoll_loop.c` en caminos de error.

Corregidos (no reintroducir): `database.c` ya cierra `db_t` siempre y filtra por namespace; `Makefile clean` fixed; `db_selector_t.skip_blob` en `microdb` evita cargar blobs completos al listar/contar; **crítico** — `src/io.c` define `open`/`close`/`read`/`write` con nombre igual a libc y visibilidad global, lo que interponía sobre las llamadas equivalentes dentro de `libmicrodb.so` (rompía `db_open` en cualquier base de datos nueva) — corregido con `-fvisibility=hidden` en el `Makefile`, **no lo quites**; query string ya soportado (`query.c`, `cl->query`), ya no rompe el ruteo.

Lista completa y priorizada: `docs/Architecture.md` (sección "Fallos Internos Relevantes", "Riesgo Crítico: Interposición de Símbolos" y "Mejoras Prioritarias").

## Reglas de memoria/ownership (relevantes para cualquier cambio en `requests.c`, `database.c`, `str.c`)

1. Si un puntero se guarda en `hash_map`, no se debe realocar su `string_pool` después.
2. Si un módulo abre un recurso externo, debe cerrarlo en el mismo módulo.
3. Un puntero devuelto desde `string_pool` deja de ser válido tras `string_pool_destroy` o `string_pool_relloc`.
4. Toda respuesta construida con `string_pool` debe escribirse antes de destruir el pool.

Detalle: `docs/Memory.md`.

## Contrato HTTP actual (no HTTP/1.1 completo)

- Puerto `5050`, siempre `Connection: close`, raíz estática en `templates/`.
- `GET /` → `templates/index.html`; `GET /<archivo>` sirve estático vía `sendfile` con `Content-Length`; `.mp4` usa `Transfer-Encoding: chunked` en fragmentos de 256 bytes.
- `GET /database/<id>` y `POST /database` (JSON mínimo, requiere campo `"key"`) hablan con `microdb`, namespace `database`.
- `GET /database/namespace/<nombre>` lista todos los registros de un namespace (`200` con `items:[]` si está vacío, nunca `404`).
- `POST /database/notes` (body raw markdown) y `GET /database/notes` (lista namespace `notes`) — feature de notas ya implementada.
- Query string soportado (`?a=1&b=2`) sin romper el ruteo: se parsea antes de despachar y queda en `cl->query`; ningún handler lo consume todavía.
- No hay: HTTP/1.1 completo, keep-alive real, routing formal, multipart, validación JSON completa, `405`, `411`, `413`.

Contrato completo con ejemplos de request/response: `docs/HTTP.md`. Integración con microdb (API usada, formato de registros, plan de migración desde sqlite del proyecto `old`): `docs/MicroDB-Integration.md`.

## Pruebas mínimas obligatorias antes de dar por terminado un cambio

```bash
make
curl -v http://localhost:5050/
curl -v http://localhost:5050/index.html
curl -v http://localhost:5050/no-existe
curl -v http://localhost:5050/../etc/passwd
curl -v -X POST http://localhost:5050/database -d '{"key":"uno","v":1}'
curl -v http://localhost:5050/database/uno
```

Casos esperados: `200` en archivo existente, `404` en inexistente, traversal debería ser `403` (el validador actual es débil — no lo asumas seguro), video usa chunked, `POST` acepta JSON mínimo con `"key"`.

Hay un script de smoke test en `scripts/http_smoke.sh` y colecciones de `/http_handler` en `.workspace/collections/sys_http.lua`. Detalle y casos adicionales: `docs/Testing.md`.

## Roadmap (para saber si un cambio pedido encaja en una fase ya planeada)

1. **Fase 1** — corregir fallos internos (`db_close` ✅, interposición de símbolos ✅, `skip_blob` ✅, `parse_headers`, path traversal, `write_headers`, doble cierre fd, `string_n_copy`).
2. **Fase 2** — separar `requests.c` en parser / router / handlers (`handlers.c`, `files.c` propios — siguen vacíos/stub).
3. **Fase 3** — HTTP/1.1 básico correcto (query string ✅, `405`, `411`, `413`, validación de versión/Content-Type).
4. **Fase 4** — `microdb` como store principal (namespaces explícitos, formato de registros `files`/`notes`/`media`).
5. **Fase 5** — migrar funciones del proyecto `old` (`/music`, `/files/<id>`, `/notes`, multipart) sin portar su arquitectura (nada de sqlite, CGI por env vars, thread pool C++).
6. **Fase 6** — pruebas repetibles (scripts curl, colecciones http_handler, casos JSON escapado).
7. **Fase 7** — observabilidad educativa (logs por conexión/request/status, sin framework).

Detalle completo: `docs/Roadmap.md`.

## Prioridades del proyecto (en orden)

1. Aprendizaje técnico
2. Correctitud HTTP/1.1
3. Estabilidad
4. Modularidad
5. Integración con pruebas desde Neovim (`/http_handler`)
6. Documentación por componente
7. Rendimiento

## Estado del repo (para no confundir con docs desactualizados)

- Rama principal para PRs: `main`. La rama de trabajo activa cambia (revisa `git branch --show-current`, no asumas `develop`).
- `http_handler` en la raíz es un **symlink** a `~/git/lua_projects/http_handler/` (herramienta Lua/Neovim para pruebas HTTP visuales, solo lectura — no editar su código) — existe aunque `docs/Testing.md` diga lo contrario.
- `scripts/http_smoke.sh` y `.workspace/` (config Neovim/debug/colecciones REST) ya existen en el repo. `scripts/http_smoke.sh` siempre borra `data.wal`/`base.db`/`blobs` antes de arrancar el servidor — no lo quites, es la única red que detecta bugs de "primer arranque" (ver interposición de símbolos arriba).
- `microdb/` es un directorio normal versionado dentro de este mismo repo (ya no un submódulo/gitlink roto). No asumas que necesita `git submodule`.
- Antes de dar por buena una prueba manual con `curl`, exporta `LD_LIBRARY_PATH="$PWD/bin"` (o usa el smoke script, que ya lo hace) — si no, `./bin/server` falla con `error while loading shared libraries: libmicrodb.so`.
