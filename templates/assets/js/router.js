// =======================
// Router modular
// =======================

const routes = {
  "#/dashboard": "components/dashboard.html",
  "#/finanzas": "components/finanzas.html",
  "#/habitos": "components/habitos.html"
};

// Aquí almacenaremos los callbacks por ruta
const routeCallbacks = {};

/**
 * Registra una función a ejecutar cuando se carga una ruta.
 * @param {string} hash - Ej: "#/habitos"
 * @param {function} callback - Función a ejecutar tras cargar la vista
 */
export function registerRouteCallback(hash, callback) {
  routeCallbacks[hash] = callback;
}

/**
 * Carga la vista según el hash actual y ejecuta su callback
 */
export async function routeChange() {
  const content = document.getElementById("content");
  const path = routes[location.hash] || routes["#/dashboard"];

  // Animación de salida
  content.classList.remove("visible");

  // Carga del componente HTML
  const res = await fetch(path);
  content.innerHTML = await res.text();

  // Actualiza el menú activo
  document.querySelectorAll("#sidebar a").forEach(a =>
    a.classList.toggle("active", a.getAttribute("href") === location.hash)
  );

  // Espera un frame antes del fade-in
  requestAnimationFrame(() => content.classList.add("visible"));

  console.debug(routeCallbacks);
  // Ejecuta el callback de la ruta si existe
  if (routeCallbacks[location.hash]) {
    try {
      console.debug(location.hash)
      setTimeout(routeCallbacks[location.hash](), 200);
    } catch (err) {
      console.error("Error en callback de ruta:", err);
    }
  }
}

// Inicialización principal
window.addEventListener("DOMContentLoaded", routeChange);
window.addEventListener("hashchange", routeChange);

