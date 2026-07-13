import { routeChange, registerRouteCallback } from "./router.js";
import { inicializarPantallaHabitos } from "./modules/habitos.js";
import { inicializarFinanzas } from "./modules/finanzas.js";

window.addEventListener("DOMContentLoaded", async () => {
  // Carga los elementos base
  await loadComponent("sidebar", "components/sidebar.html");
  await loadComponent("header", "components/header.html");

  // Rutas dinámicas
  routeChange();
  window.addEventListener("hashchange", routeChange);

  // Fecha dinámica
  document.getElementById("fecha").textContent = new Date().toLocaleDateString("es-ES", {
    weekday: "long",
    year: "numeric",
    month: "long",
    day: "numeric"
  });

  // Registra callbacks de vistas
  registerRouteCallback("#/habitos", inicializarPantallaHabitos);
  registerRouteCallback("#/finanzas", inicializarFinanzas);
});

async function loadComponent(id, path) {
  const container = document.getElementById(id);
  const res = await fetch(path);
  container.innerHTML = await res.text();
}

// Escucha el envío del formulario de finanzas
document.addEventListener("submit", e => {
  if (e.target.id === "formFinanza") {
    e.preventDefault();
    const tipo = e.target.tipo.value;
    const descripcion = e.target.descripcion.value.trim();
    const monto = parseFloat(e.target.monto.value);
    if (!descripcion || isNaN(monto)) return;

    const fecha = new Date().toLocaleDateString("es-ES");
    const tabla = document.querySelector("#tablaFinanzas tbody");
    const fila = document.createElement("tr");
    fila.innerHTML = `
      <td>${fecha}</td>
      <td>${tipo}</td>
      <td>${descripcion}</td>
      <td>${tipo === "Gasto" ? "−" : "+"} $${monto.toFixed(2)}</td>
    `;
    tabla.prepend(fila);

    showToast(`Transacción "${descripcion}" guardada`);
    e.target.reset();
  }
});

// Notificación temporal
function showToast(msg) {
  const toast = document.createElement("div");
  toast.className = "toast";
  toast.textContent = msg;
  document.body.appendChild(toast);
  setTimeout(() => toast.remove(), 3000);
}

