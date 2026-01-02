function inicializarFormularioHabitos() {
  const encabezado = document.getElementById("encabezadoDias");
  const cuerpo = document.getElementById("cuerpoHabitos");
  const mesActual = document.getElementById("mesActual");
  const inputHabito = document.getElementById("nuevoHabito");
  const btnAgregar = document.getElementById("btnAgregarHabito");


  if (!encabezado || !cuerpo) {
    console.debug("NULL");
    return;
  }

  // ===Generar encabezado dinámico ===
  const hoy = new Date();
  const mesNombre = hoy.toLocaleString("es-ES", { month: "long", year: "numeric" });
  mesActual.textContent = mesNombre.charAt(0).toUpperCase() + mesNombre.slice(1);

  const diasSemana = obtenerDiasSemana(hoy);
  diasSemana.forEach(dia => {
    const th = document.createElement("th");
    th.textContent = dia.getDate();
    encabezado.appendChild(th);
  });

  // ===Estado de hábitos (inicial vacío) ===
  const habitos = [];


  // ===Agregar hábito ===
  btnAgregar.onclick = () => {
    console.debug("CLICK");
    const nombre = inputHabito.value.trim();
    if (!nombre) return;
    habitos.push({ nombre, dias: Array(7).fill(false) });
    inputHabito.value = "";
    renderHabitos();
  };

  // ===Renderizar tabla ===
  function renderHabitos() {
    cuerpo.innerHTML = "";
    habitos.forEach((habito, i) => {
      console.debug(habito);
      const fila = document.createElement("tr");
      const celdaNombre = document.createElement("td");
      celdaNombre.textContent = habito.nombre;

      console.debug(cuerpo);
      fila.appendChild(celdaNombre);

      habito.dias.forEach((valor, d) => {
        const td = document.createElement("td");
        const check = document.createElement("input");
        check.type = "checkbox";
        check.checked = valor;
        check.addEventListener("change", () => {
          habito.dias[d] = check.checked;
        });
        td.appendChild(check);
        fila.appendChild(td);
      });

      cuerpo.appendChild(fila);
    });
  }

  // ===Genera los días de la semana actual (lunes a domingo) ===
  function obtenerDiasSemana(fecha) {
    const inicioSemana = new Date(fecha);
    const diaSemana = inicioSemana.getDay(); // 0 = domingo, 1 = lunes...
    const offset = diaSemana === 0 ? -6 : 1 - diaSemana; // mover a lunes
    inicioSemana.setDate(inicioSemana.getDate() + offset);

    return Array.from({ length: 7 }, (_, i) => {
      const d = new Date(inicioSemana);
      d.setDate(inicioSemana.getDate() + i);
      return d;
    });
  }

  renderHabitos();
  console.debug("Formulario de hábitos inicializado correctamente");
}

export function inicializarPantallaHabitos() {
  inicializarFormularioHabitos();
}
