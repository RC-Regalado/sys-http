# Memoria y Ownership

## Modelo General

El proyecto evita `malloc` de libc en el servidor principal. La memoria se administra con `mmap` mediante `sysmap_alloc` y `sysmap_free`.

`microdb` es una excepcion parcial: se compila como libreria C independiente y puede usar su propia capa interna de utilidades.

## sysmap_alloc

`sysmap_alloc(size)` reserva:

```txt
[sysmap_header][payload]
```

El tamano real se alinea a 4096 bytes. Esto simplifica `munmap`, pero desperdicia memoria para objetos pequenos.

Ownership:

* quien llama `sysmap_alloc` debe llamar `sysmap_free`
* `string_pool_destroy` libera la arena interna
* `client_destroy` libera el pool y el cliente

## string_pool

`string_pool` es una arena lineal para strings.

Campos:

* `base`: memoria base
* `capacity`: capacidad total
* `offset`: siguiente posicion libre
* `mark`: punto de retorno para resets parciales

Funciones:

* `string_pool_init`
* `string_pool_alloc`
* `string_pool_nalloc`
* `string_pool_append`
* `string_pool_reset`
* `string_pool_destroy`
* `string_pool_mark`
* `string_pool_reset_to_mark`
* `string_pool_format`

Uso actual:

* almacenamiento de headers
* rutas temporales
* cuerpos HTTP pequenos
* generacion de respuestas JSON

## Riesgos de Memoria

* `string_pool_relloc` mueve `base`; punteros anteriores quedan invalidos.
* `read_body` guarda punteros dentro de `cl->pool`; cualquier realloc posterior puede invalidar valores ya insertados en `hash_map`.
* `database.c` no llama `db_close`, dejando ownership incompleto.
* `hash_map` almacena punteros externos, no copia strings por si mismo.
* `substr` no agrega terminador nulo.
* `string_n_copy` no termina si el buffer se llena.
* `json_add_object` confia en que el valor entrante sea JSON valido.

## Reglas Practicas

1. Si un puntero se guarda en `hash_map`, no se debe realocar su pool despues.
2. Si un modulo abre un recurso externo, debe cerrarlo en el mismo modulo.
3. Si una funcion devuelve puntero a `string_pool`, su validez termina con `string_pool_destroy` o `string_pool_relloc`.
4. Si una respuesta se construye con `string_pool`, se debe escribir antes de destruir el pool.

## Mejoras Recomendadas

* Agregar `string_pool_reserve` y usarlo antes de insertar punteros en `hash_map`.
* Corregir `database.c` para llamar `db_close`.
* Hacer que `hashmap_put` retorne error si se llena.
* Hacer que `string_n_copy` siempre escriba `\0` si `n > 0`.
* Documentar explicitamente si cada funcion transfiere ownership o solo presta punteros.

## Pruebas Sugeridas

```bash
curl -v -X POST http://localhost:5050/database -d '{"key":"a"}'
curl -v -X POST http://localhost:5050/database -d '{"key":"escape","value":"a\"b"}'
curl -v -X POST http://localhost:5050/database -d '{"key":"large","value":"xxxxxxxx"}'
```

Para pruebas de memoria mas profundas, usar un build alternativo con sanitizers solo si no contradice el objetivo freestanding.

