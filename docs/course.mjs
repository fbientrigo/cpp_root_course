import {ACHIEVEMENTS, blank, normalize, merge, load, save} from './progress.mjs';
const lessons = [
  ['Movimiento y contratos', 'Conecta objetos existentes y predice cómo cambia su estado.', 'Implementar el movimiento del jugador con una entrada ya normalizada. El mapa y el dibujo vienen preparados.', 'Explorar una clase, construir un TH1D y llenar una distribución de pT.', 'levels/01/lesson.cpp', 'Si pasas Position por valor, ¿qué posición dibujará el motor al volver? ¿Qué debe pasar frente a una pared?'],
  ['Clases y encapsulamiento', 'Diseña estado privado, constructor y métodos con invariantes.', 'Construir la criatura: recibir daño sin permitir vitalidad negativa. Pueden agregar propiedades propias conservando el pequeño contrato.', 'Comparar tu clase con un objeto de ROOT. Inspeccionar estado y métodos sin memorizar nombres.', 'levels/02/creature.hpp', '¿Quién debe impedir que la vitalidad sea negativa? ¿Qué operaciones necesitan acceso de escritura?'],
  ['Colecciones y referencias', 'Distingue copia, referencia y puntero prestado.', 'Implementar la captura dentro de un equipo con capacidad de seis. Conservar una copia propia y manejar un objetivo ausente.', 'Recorrer eventos y llenar histogramas. Comparar el acceso con punto y flecha.', 'levels/03/lesson.cpp', 'Si guardas la dirección de la criatura del encuentro, ¿qué ocurre cuando comienza el siguiente?'],
  ['Selección y masa invariante', 'Expresa un criterio y observa cómo transforma la muestra.', 'Seleccionar pares de cargas opuestas con ambos pT sobre un umbral. La masa usa la suma de cuadrimomentos.', 'Comparar el espectro de masa antes y después de los cortes. Investigar la forma de la distribución.', 'levels/04/lesson.cpp', '¿Un corte de pT puede deformar el espectro de masa? Predice qué perderás antes de ejecutarlo.'],
  ['Persistencia y TTree', 'Conserva la información y verifica su esquema.', 'Completar el registro de eventos sin alterar valores ni perder filas. Separar el escritor del modelo.', 'Convertir el CSV a un TTree real, inspeccionar ramas, escribir un TFile y volver a abrirlo.', 'levels/05/lesson.cpp', '¿Qué información necesitas guardar para poder cambiar un corte mañana sin repetir la generación?'],
  ['Análisis autónomo', 'Formula una pregunta, programa una selección y explica el resultado.', 'Implementar una ventana de masa y después una variante propia. Comparar resultados con una semilla fija.', 'Procesar el conjunto sintético, comparar elecciones y exportar una figura. Explicar límites y sesgos.', 'levels/06/lesson.cpp', 'Una acumulación en la masa esperada: ¿qué muestra y qué no permite concluir sobre su origen?']
];
const $ = id => document.getElementById(id);
let state, active, audio, sound = false, persistenceBlocked = false;
function notice(message) { $('notice').textContent = message; }
try { state = load(localStorage); } catch (error) {
  state = {active: 'alumno', profiles: [blank('alumno')]}; persistenceBlocked = true;
  notice('No se pudo recuperar el guardado: ' + error.message + ' Exporta o importa una copia; el dato previo no se sobrescribirá.');
}
function persist() {
  if (persistenceBlocked) { $('saveState').textContent = 'SOLO EN MEMORIA'; return; }
  try { save(localStorage, state); $('saveState').textContent = 'GUARDADO LOCAL'; }
  catch { $('saveState').textContent = 'SOLO EN MEMORIA'; notice('El navegador no permite guardar. Exporta tu progreso antes de cerrar.'); }
}
function tone() {
  if (!sound) return;
  try {
    audio ||= new (window.AudioContext || window.webkitAudioContext)();
    audio.resume();
    [523.25, 659.25, 783.99].forEach((hz, i) => {
      const oscillator = audio.createOscillator(), volume = audio.createGain();
      const start = audio.currentTime + i * .08;
      oscillator.frequency.value = hz; oscillator.type = 'sine';
      volume.gain.setValueAtTime(0, start); volume.gain.linearRampToValueAtTime(.07, start + .01);
      volume.gain.exponentialRampToValueAtTime(.001, start + .12);
      oscillator.connect(volume); volume.connect(audio.destination); oscillator.start(start); oscillator.stop(start + .13);
    });
  } catch { notice('El navegador no ofrece audio. El progreso sigue disponible.'); }
}
function current() { return state.profiles.find(p => p.student === state.active); }
function render() {
  const profile = current(); active = profile.lastSession;
  $('student').replaceChildren(...state.profiles.map(p => { const o = new Option(p.student, p.student); o.selected = p.student === state.active; return o; }));
  const count = Object.values(profile.sessions).reduce((n, s) => n + Object.values(s).filter(Boolean).length, 0);
  $('completion').textContent = count + ' / 18 pasos'; $('progressBar').value = count;
  $('sessions').replaceChildren(...lessons.map((lesson, index) => {
    const id = index + 1, done = Object.values(profile.sessions[id]).filter(Boolean).length;
    const button = document.createElement('button'); button.className = 'session';
    button.setAttribute('aria-pressed', String(id === active));
    button.innerHTML = `<span class="session-top"><span>S0${id} / SEMANA ${Math.ceil(id/2)}</span><span>${done === 3 ? 'COMPLETA' : done ? 'EN CURSO' : 'POR COMENZAR'}</span></span><strong>${lesson[0]}</strong><span class="session-progress">${done}/3 pasos</span><svg viewBox="0 0 100 20" aria-hidden="true"><path class="wave" d="M0 12H12L16 11L20 14L24 3L28 17L32 8L36 12H100" fill="none" stroke="currentColor" stroke-width="1.3"/></svg>`;
    button.addEventListener('click', () => { profile.lastSession = id; persist(); render(); }); return button;
  }));
  const lesson = lessons[active - 1];
  $('sessionTag').textContent = 'S0' + active; $('lessonWeek').textContent = 'SEMANA ' + Math.ceil(active/2) + ' / CONSTRUIR → OBSERVAR → EXPLICAR';
  $('lessonTitle').textContent = lesson[0]; $('lessonGoal').textContent = lesson[1]; $('lessonPair').textContent = lesson[2];
  $('lessonRoot').textContent = lesson[3]; $('lessonFile').textContent = lesson[4]; $('lessonPredict').textContent = lesson[5];
  for (const key of ['prepare', 'pair', 'root']) $(key).checked = profile.sessions[active][key];
  $('lessonCommand').textContent = `python3 course.py play ${active} --student ${profile.student}\npython3 course.py check ${active} --student ${profile.student}`;
  $('badges').replaceChildren(...Object.entries(ACHIEVEMENTS).map(([id, label]) => {
    const earned = profile.achievements.includes(id), badge = document.createElement('div');
    badge.className = 'badge' + (earned ? ' earned' : '');
    const icon = document.createElement('span'); icon.textContent = earned ? '✓' : '○'; icon.setAttribute('aria-hidden', 'true');
    badge.append(icon, document.createTextNode(label + (earned ? ' · OBTENIDO' : ' · PENDIENTE'))); return badge;
  }));
  if (persistenceBlocked) $('saveState').textContent = 'SOLO EN MEMORIA';
}
$('student').addEventListener('change', event => { state.active = event.target.value; persist(); render(); notice('Perfil activo: ' + state.active); });
$('profileForm').addEventListener('submit', event => {
  event.preventDefault();
  try {
    const profile = blank($('newStudent').value.trim());
    if (!state.profiles.some(p => p.student === profile.student)) {
      if (state.profiles.length >= 100) throw new Error('Máximo de 100 perfiles por navegador.');
      state.profiles.push(profile);
    }
    state.active = profile.student; persist(); render(); $('newStudent').value = ''; notice('Perfil activo: ' + state.active);
  } catch (error) { notice(error.message); }
});
for (const key of ['prepare', 'pair', 'root']) $(key).addEventListener('change', event => {
  current().sessions[active][key] = event.target.checked; if (event.target.checked) tone();
  persist(); render(); notice('Paso actualizado para ' + state.active + '. Es seguimiento autodeclarado.');
});
$('sound').addEventListener('click', () => { sound = !sound; $('sound').setAttribute('aria-pressed', String(sound)); $('sound').textContent = 'SONIDO: ' + (sound ? 'ON' : 'OFF'); if (sound) tone(); });
$('export').addEventListener('click', () => {
  const profile = normalize(current());
  const url = URL.createObjectURL(new Blob([JSON.stringify(profile, null, 2)], {type: 'application/json'}));
  const a = document.createElement('a'); a.href = url; a.download = 'progreso-' + profile.student + '.json'; a.click();
  setTimeout(() => URL.revokeObjectURL(url), 1000); notice('Copia exportada. No contiene datos de otros perfiles.');
});
$('import').addEventListener('change', async event => {
  try {
    const file = event.target.files[0]; if (!file) return;
    if (file.size > 1000000) throw new Error('El archivo supera 1 MB.');
    const profile = normalize(JSON.parse(await file.text()));
    const index = state.profiles.findIndex(p => p.student === profile.student);
    if (index >= 0) state.profiles[index] = merge(state.profiles[index], profile);
    else { if (state.profiles.length >= 100) throw new Error('Máximo de 100 perfiles.'); state.profiles.push(profile); }
    state.active = profile.student;
    // Un error de lectura previo conserva el dato original hasta que el usuario exporte
    // y recupere el almacenamiento por su cuenta; nunca borrarlo silenciosamente.
    persist(); render(); tone(); notice('Progreso combinado para ' + profile.student + '. Se conservaron los pasos y logros previos.');
  } catch (error) { notice('No se importó: ' + error.message); }
  event.target.value = '';
});
$('copy').addEventListener('click', async () => {
  try { await navigator.clipboard.writeText($('lessonCommand').textContent); notice('Comandos copiados. En Windows nativo puedes usar python en lugar de python3.'); }
  catch { notice('Selecciona y copia los comandos del panel.'); }
});
render();
