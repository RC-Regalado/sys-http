# 📘 Guía de Etiquetas HTML — Referencia Esencial

> **Objetivo:** resumen práctico de las etiquetas HTML más usadas, agrupadas por función.

---

## 🧱 Estructura básica del documento

| Etiqueta | Uso principal | Ejemplo |
|-----------|----------------|----------|
| `<!DOCTYPE html>` | Indica que el documento es HTML5 | `<!DOCTYPE html>` |
| `<html>` | Raíz del documento | `<html lang="es">` |
| `<head>` | Contiene metadatos, scripts y estilos | `<head>...</head>` |
| `<body>` | Contiene el contenido visible de la página | `<body>...</body>` |

---

## 🧭 Metadatos y configuración

| Etiqueta | Descripción | Ejemplo |
|-----------|--------------|----------|
| `<meta>` | Define metadatos (charset, viewport, descripción, etc.) | `<meta charset="UTF-8">` |
| `<title>` | Título mostrado en la pestaña del navegador | `<title>Mi sitio</title>` |
| `<link>` | Enlaza hojas de estilo u otros recursos | `<link rel="stylesheet" href="style.css">` |
| `<script>` | Incluye o referencia código JavaScript | `<script src="main.js"></script>` |
| `<style>` | Define estilos internos | `<style>body { color: red; }</style>` |

---

## 📰 Estructura semántica

| Etiqueta | Propósito | Ejemplo |
|-----------|------------|----------|
| `<header>` | Cabecera de una página o sección | `<header>...</header>` |
| `<nav>` | Contenedor para enlaces de navegación | `<nav><a href="#">Inicio</a></nav>` |
| `<main>` | Contenido principal del documento | `<main>...</main>` |
| `<section>` | Agrupa contenido temático | `<section>Noticias</section>` |
| `<article>` | Contenido independiente (post, artículo) | `<article>...</article>` |
| `<aside>` | Contenido lateral o complementario | `<aside>Publicidad</aside>` |
| `<footer>` | Pie de página | `<footer>© 2025</footer>` |
| `<div>` | Contenedor genérico sin valor semántico | `<div class="contenedor"></div>` |

---

## 🧾 Texto y formato

| Etiqueta | Función | Ejemplo |
|-----------|----------|----------|
| `<h1>`–`<h6>` | Encabezados de diferentes niveles | `<h1>Título</h1>` |
| `<p>` | Párrafo de texto | `<p>Hola mundo</p>` |
| `<span>` | Fragmento en línea sin semántica | `<span class="resaltado">texto</span>` |
| `<br>` | Salto de línea | `Hola<br>Mundo` |
| `<hr>` | Línea divisoria | `<hr>` |
| `<strong>` | Énfasis fuerte (semántico) | `<strong>Importante</strong>` |
| `<em>` | Énfasis (cursiva semántica) | `<em>Texto</em>` |
| `<b>` | Negrita visual | `<b>Texto</b>` |
| `<i>` | Cursiva visual | `<i>Texto</i>` |
| `<u>` | Subrayado | `<u>Texto</u>` |
| `<mark>` | Resalta texto | `<mark>Palabra clave</mark>` |
| `<small>` | Texto de menor tamaño | `<small>nota</small>` |
| `<blockquote>` | Cita larga | `<blockquote>Cita famosa...</blockquote>` |
| `<pre>` | Texto preformateado | `<pre>code block</pre>` |
| `<code>` | Fragmento de código | `<code>console.log()</code>` |

---

## 📋 Listas

| Tipo | Etiquetas | Ejemplo |
|------|------------|----------|
| Ordenada | `<ol>`, `<li>` | `<ol><li>Uno</li><li>Dos</li></ol>` |
| No ordenada | `<ul>`, `<li>` | `<ul><li>A</li><li>B</li></ul>` |
| Definición | `<dl>`, `<dt>`, `<dd>` | `<dl><dt>Término</dt><dd>Definición</dd></dl>` |

---

## 🔗 Enlaces e imágenes

| Etiqueta | Descripción | Ejemplo |
|-----------|--------------|----------|
| `<a>` | Enlace a otra página o sección | `<a href="about.html">Acerca</a>` |
| `<img>` | Muestra una imagen | `<img src="foto.jpg" alt="Descripción">` |
| `<figure>` | Contenedor de imagen con pie | `<figure><img><figcaption>Texto</figcaption></figure>` |

---

## 🧮 Tablas

| Etiqueta | Propósito | Ejemplo |
|-----------|------------|----------|
| `<table>` | Define una tabla | `<table>...</table>` |
| `<tr>` | Fila de tabla | `<tr>...</tr>` |
| `<th>` | Celda de encabezado | `<th>Columna</th>` |
| `<td>` | Celda de datos | `<td>Valor</td>` |
| `<thead>`, `<tbody>`, `<tfoot>` | Agrupan partes de la tabla | `<thead>...</thead>` |

---

## 🧩 Formularios

| Etiqueta | Función | Ejemplo |
|-----------|----------|----------|
| `<form>` | Contenedor de formulario | `<form action="enviar.php"></form>` |
| `<input>` | Campo de entrada (texto, número, email, etc.) | `<input type="text">` |
| `<textarea>` | Área de texto multilínea | `<textarea></textarea>` |
| `<select>` | Menú desplegable | `<select><option>1</option></select>` |
| `<option>` | Opción dentro de `<select>` | `<option value="A">A</option>` |
| `<label>` | Etiqueta asociada a un campo | `<label for="nombre">Nombre:</label>` |
| `<button>` | Botón clicable | `<button>Enviar</button>` |
| `<fieldset>` | Agrupa campos relacionados | `<fieldset><legend>Datos</legend></fieldset>` |
| `<legend>` | Título de un `<fieldset>` | `<legend>Información</legend>` |

---

## 🎥 Multimedia

| Etiqueta | Uso | Ejemplo |
|-----------|-----|----------|
| `<audio>` | Reproduce sonido | `<audio controls src="musica.mp3"></audio>` |
| `<video>` | Reproduce video | `<video controls src="video.mp4"></video>` |
| `<source>` | Fuente alternativa de audio/video | `<source src="file.mp4">` |
| `<canvas>` | Lienzo para gráficos con JS | `<canvas id="chart"></canvas>` |
| `<svg>` | Gráficos vectoriales | `<svg><circle cx="50" cy="50" r="40"/></svg>` |

---

## 🧠 Elementos interactivos y avanzados

| Etiqueta | Uso | Ejemplo |
|-----------|-----|----------|
| `<details>` | Sección colapsable | `<details><summary>Ver más</summary>Texto oculto</details>` |
| `<summary>` | Título de un `<details>` | `<summary>Ver más</summary>` |
| `<dialog>` | Cuadro de diálogo modal | `<dialog open>Mensaje</dialog>` |
| `<template>` | Contenido HTML no renderizado | `<template><div>Reutilizable</div></template>` |
| `<slot>` | Punto de inserción en Web Components | `<slot></slot>` |

---

## ⚙️ Atributos comunes

| Atributo | Descripción | Ejemplo |
|-----------|--------------|----------|
| `id` | Identificador único | `<div id="header"></div>` |
| `class` | Clase CSS | `<div class="card"></div>` |
| `style` | Estilo en línea | `<p style="color:red;">Texto</p>` |
| `src` | Fuente de recurso (imagen, script, etc.) | `<img src="foto.jpg">` |
| `href` | Enlace a destino | `<a href="index.html"></a>` |
| `alt` | Texto alternativo (imágenes) | `<img alt="Logo">` |
| `title` | Texto emergente al pasar el cursor | `<span title="Ayuda">?</span>` |
| `disabled` | Desactiva un control | `<button disabled>OK</button>` |
| `checked` | Marca una casilla o radio | `<input type="checkbox" checked>` |
| `placeholder` | Texto guía en inputs | `<input placeholder="Correo">` |

---

## 🧩 Recursos útiles

- [MDN HTML Reference](https://developer.mozilla.org/es/docs/Web/HTML/Element)
- [W3C HTML Spec](https://html.spec.whatwg.org/)
- [HTML Cheat Sheet PDF](https://htmlcheatsheet.com/)

---

📚 **Consejo de práctica:**  
Construye pequeñas páginas temáticas usando un grupo de etiquetas por día.  
Por ejemplo: “día de tablas”, “día de formularios”, “día de layouts semánticos”.


