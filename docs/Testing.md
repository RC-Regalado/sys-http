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

Payload sin key:

```bash
curl -v -X POST http://localhost:5050/database -d '{"v":1}'
```

Payload con comillas escapadas:

```bash
curl -v -X POST http://localhost:5050/database -d '{"key":"escape","value":"a\"b"}'
```

## Resultado Esperado

* `/` retorna `200 OK`.
* archivo inexistente retorna `404`.
* traversal debe retornar `403`, aunque el validador actual necesita endurecerse.
* `POST /database` con `"key"` retorna JSON con `"ok":true`.
* `GET /database/<key>` retorna JSON con `"found":true`.
* `POST /database` sin `"key"` retorna error JSON.

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

