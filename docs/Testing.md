# Pruebas

## Objetivo

Las pruebas deben validar el comportamiento observable sin ocultar el enfoque educativo del proyecto. La herramienta principal es `curl`; `http_handler` puede usarse desde Neovim si esta disponible.

## Compilacion

```bash
make
```

Prueba de `microdb`:

```bash
make -C microdb
make -C microdb test
```

## Servidor

Ejecutar:

```bash
./bin/server
```

Puerto esperado:

```txt
5050
```

**Importante**: probar al menos una vez contra una base de datos totalmente nueva antes de dar por buena una prueba de `/database*`:

```bash
rm -f data.wal base.db && rm -rf blobs
./bin/server
```

Un `data.wal`/`base.db` preexistente (de una sesion anterior) puede enmascarar bugs que solo aparecen cuando `microdb` arranca desde cero — asi paso desapercibido durante meses el bug de interposicion de simbolos documentado en `docs/Architecture.md`.

## Casos GET

Archivo raiz:

```bash
curl -v http://localhost:5050/
```

Archivo explicito:

```bash
curl -v http://localhost:5050/index.html
```

Archivo inexistente:

```bash
curl -v http://localhost:5050/no-existe
```

Path traversal:

```bash
curl -v http://localhost:5050/../etc/passwd
```

Video:

```bash
curl -v http://localhost:5050/hollow.mp4
```

## Casos POST

Guardar JSON:

```bash
curl -v -X POST http://localhost:5050/database -d '{"key":"uno","v":1}'
```

Leer JSON guardado:

```bash
curl -v http://localhost:5050/database/uno
```

Listar registros de un namespace:

```bash
curl -v http://localhost:5050/database/namespace/database
```

Listar un namespace sin registros (sigue siendo `200`, `items` vacio):

```bash
curl -v http://localhost:5050/database/namespace/no-existe
```

Payload sin key:

```bash
curl -v -X POST http://localhost:5050/database -d '{"v":1}'
```

Payload con comillas escapadas:

```bash
curl -v -X POST http://localhost:5050/database -d '{"key":"escape","value":"a\"b"}'
```

## Prueba con navegador real

`curl` no reproduce todo: cabeceras grandes/fragmentadas y el timing real de un navegador expusieron bugs que `curl` no disparaba (ver `docs/Architecture.md`, seccion "Riesgo Critico: SIGSEGV en `read_incoming`..."). Al probar en Firefox/Chrome real:

* Si un build con un bug conocido (contenido truncado, `Content-Length` incorrecto, etc.) ya sirvio una URL en un perfil de navegador, ese perfil puede quedar con **una entrada de cache de disco corrupta o parcial** para esa URL. Un fix posterior en el servidor puede parecer que no funciona (ej. `NS_ERROR_NET_RESET` en Firefox) cuando en realidad el servidor ya esta bien y el navegador esta revalidando/completando una entrada de cache rota. Antes de reabrir investigacion sobre un bug "que persiste" tras un fix: **probar en una ventana privada o un perfil nuevo**; si ahi funciona, el problema era la cache del perfil usado para las pruebas anteriores, no el servidor (ver postmortem completo en `docs/Architecture.md`).
* `scripts/http_smoke.sh` ya cubre cabeceras fragmentadas tipo Firefox y descargas grandes con cliente lento, pero corre contra un perfil de cache limpio (`curl` no cachea) — no sustituye una prueba manual en navegador real cuando el cambio toca `files.c`/servir estaticos.

## Resultado Esperado

* `/` retorna `200 OK`.
* archivo inexistente retorna `404`.
* traversal debe retornar `403`, aunque el validador actual necesita endurecerse.
* `POST /database` con `"key"` retorna JSON con `"ok":true`.
* `GET /database/<key>` retorna JSON con `"found":true`.
* `POST /database` sin `"key"` retorna error JSON.
* `GET /database/namespace/<nombre>` retorna `200` con `"items":[...]`, incluso vacio si el namespace no tiene registros.

## Fallos Internos que Deben Cubrirse

* headers sin `:` no deben romper `parse_headers`.
* body mayor que `CLIENT_BUF_SIZE` debe tener limite explicito o error controlado.
* rutas con `../` en segmentos internos deben bloquearse.
* `database.c` debe cerrar `db_t` despues de leer o escribir.
* `write_headers` debe tener contrato consistente para todos los status.
* `POST` con JSON malformado debe responder `400`, no `500`.

## http_handler

`Agents.md` menciona `/http_handler`, pero no hay carpeta `http_handler` en el estado actual del repo. Si se agrega, las colecciones minimas deberian cubrir:

* GET raiz
* GET archivo inexistente
* GET traversal
* POST database
* GET database

Estructura sugerida:

```txt
http_handler/
├── collections/
│   ├── static.http
│   └── database.http
└── environments/
    └── local.json
```

