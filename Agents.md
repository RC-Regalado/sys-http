# AGENTS.md

## Contexto

Este proyecto es un servidor HTTP educativo escrito en C, orientado a comprender sockets, `epoll`, parsing HTTP, manejo manual de memoria, estructuras de datos y comunicación con una base de datos educativa escrita también en C.

El proyecto no busca depender de frameworks ni librerías externas. Su objetivo principal es aprendizaje técnico de bajo nivel.

## Estructura del proyecto

```txt
/src              Código principal del servidor HTTP
/microdb          Base de datos educativa en C
.project-env      Variables de entorno del proyecto
.workspace        Configuración local para Neovim, debug, ejecutables y pruebas REST
/http_handler      Herramienta Lua/Neovim para pruebas HTTP visuales
```

## Módulos principales

Cada archivo C representa un módulo lógico:

```txt
client.c          Manejo de clientes/conexiones
database.c        Adaptador entre HTTP y microdb
epoll_loop.c      Loop principal con epoll
files.c           Lectura y respuesta de archivos
handlers.c        Handlers HTTP
hashmap.c         Hash map interno
io.c              Entrada/salida
json.c            Parsing mínimo JSON
memory.c          Memoria/allocators
requests.c        Parsing y representación HTTP
server.c          Inicialización del servidor
str.c             Utilidades de string
syscalls.c        Wrappers de syscalls
```

## Reglas obligatorias

1. No usar librerías externas.
2. No introducir dependencias de libc si contradicen el modo `freestanding`.
3. No romper APIs internas existentes sin documentar impacto.
4. No modificar `/microdb` sin justificación técnica explícita.
5. No reemplazar componentes educativos por soluciones de alto nivel.
6. No eliminar módulos existentes sin aprobación.
7. Mantener documentación en Markdown.
8. Toda mejora debe preservar el enfoque educativo del proyecto.
9. Antes de refactorizar, documentar el módulo afectado.
10. Antes de modificar parsing HTTP, validar con `curl` y con `/http_handler` si aplica.

## Flags relevantes de compilación

El proyecto usa compilación de bajo nivel con flags como:

```bash
-g
-ffreestanding
-fno-builtin
-nostdlib
-nostartfiles
-nodefaultlibs
-fno-stack-protector
-Wall
-Werror
-O0
```

Esto implica que no se debe asumir disponibilidad de runtime estándar.

## Flujo de trabajo para agentes

Antes de modificar código:

1. Leer este archivo.
2. Revisar `docs/Architecture.md`.
3. Identificar el módulo afectado.
4. Revisar APIs internas existentes.
5. Proponer plan breve.
6. Aplicar cambios pequeños.
7. Compilar con `make`.
8. Probar con `curl`.
9. Si aplica, probar con `/http_handler`.
10. Actualizar documentación.

## Pruebas mínimas esperadas

Validar al menos:

```bash
make
curl -v http://localhost:<puerto>/
curl -v http://localhost:<puerto>/archivo.html
curl -v http://localhost:<puerto>/no-existe
curl -v http://localhost:<puerto>/../archivo
curl -v -X POST http://localhost:<puerto>/... -d '{"key":"value"}'
```

Casos esperados:

* `GET` de archivo existente retorna HTML.
* Archivo inexistente retorna `404`.
* Ruta con `..` retorna `403`.
* Video/streaming usa `chunked`.
* Consultas a microdb funcionan mediante handlers HTTP.
* `POST` acepta JSON mínimo.

## Documentación obligatoria

Mantener documentos en:

```txt
docs/
├── Architecture.md
├── Roadmap.md
├── Components.md
├── HTTP.md
├── Memory.md
├── Testing.md
└── MicroDB-Integration.md
```

Cada módulo debe documentarse con:

* propósito
* funciones principales
* estructuras importantes
* dependencias internas
* riesgos
* ejemplos de uso
* pruebas sugeridas
* compatibilidad hacia obsidian

## Prioridades

1. Aprendizaje técnico.
2. Correctitud HTTP/1.1.
3. Estabilidad.
4. Modularidad.
5. Integración con pruebas desde Neovim.
6. Documentación por componente.
7. Rendimiento.

