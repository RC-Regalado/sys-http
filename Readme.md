# http-minimal (Linux, sin libc)

Servidor HTTP minimalista escrito en C/asm con syscalls directas. Diseñado para aprender:
- llamadas al sistema (socket, bind, accept, read/write, sendfile, setsockopt…)
- memoria con mmap/brk, arenas (`string_pool`)
- parsing manual de HTTP/1.1, headers, `Content-Length` y `Connection: close`
- E/S por bloques y zero-copy (`sendfile`)

## Como usar
```
bash
make              # compila (static, sin libc)
sudo ./server     # inicia en :5050 (aun no acepta parámetros)
curl -v localhost:5050/
```
## Características

-   GET estático con `Content-Length` + `Connection: close`
-   `SO_REUSEADDR` para reusar puerto
-   `sendfile` para zero-copy (archivos grandes)
-   `string_pool` (arena) para claves/valores y rutas
-   logging sin libc (`logf`, `%s`/`%d`)
-   (Opcional) 403 en `..` traversal

## Requisitos
-   Linux x86_64 (syscall ABI)
-   `gcc` + `make`
-   `gas`
-   `strace`, `gdb`
    
## Compilación
```
make clean && make
```
## Ejecución
```
./server               # por defecto :5050
```
## Estructura del repo
.
├── src/
 |	  ├── include/            # headers propios
│   ├── server.c        # bucle principal, accept, GET
│   ├── http.c          # parseo request line + headers
│   ├── io.c            # writef/logf, syscalls genéricas
│   ├── pool.c          # string_pool (arena)
│   ├── sys.S           # _start, syscallN, sysmap (si lo separas)
│   └── net.c           # setsockopt, sendfile wrapper, etc.
├── templates/          # archivos estáticos (index.html, css…)
├── Makefile
└── README.md

## Diseño interno (resumen)
-   **Memoria:** `sysmap_alloc/free` (mmap/munmap) + `string_pool`
-   **HTTP GET:** parseo manual; 400/404/500/403 básicos
-   **Zero-copy:** `sendfile(out=socket, in=file, &off, len)`    
-   **Seguridad:** bloqueo de `..`, normalización de ruta

