export const COURSE = 'cpp-root-course';
export const KEY = 'cpp-root-course:v1';
export const ACHIEVEMENTS = {
  first_move: 'Primer desplazamiento', first_capture: 'Primera captura',
  first_selection: 'Primer evento seleccionado', first_export: 'Primer registro exportado',
  check01: 'S01 · Movimiento verificado', check02: 'S02 · Estado protegido',
  check03: 'S03 · Equipo verificado', check04: 'S04 · Selección verificada',
  check05: 'S05 · Datos conservados', check06: 'S06 · Ventana verificada'
};
export function normalize(raw) {
  if (!raw || raw.schema !== 1 || raw.course !== COURSE || raw.demo === true)
    throw new Error('Archivo incompatible o de demostración. Usa un progreso real de este curso.');
  if (typeof raw.student !== 'string' || !/^[A-Za-z0-9_-]{1,32}$/.test(raw.student))
    throw new Error('Perfil: 1–32 letras ASCII, números, guion o guion bajo.');
  if (raw.achievements !== undefined && !Array.isArray(raw.achievements))
    throw new Error('La lista de logros no es válida.');
  const profile = {schema: 1, course: COURSE, student: raw.student, achievements: [], sessions: {}, lastSession: 1};
  profile.achievements = [...new Set((raw.achievements || []).filter(id => Object.hasOwn(ACHIEVEMENTS, id)))];
  for (let i = 1; i <= 6; i++) {
    const session = raw.sessions?.[i];
    profile.sessions[i] = {prepare: session?.prepare === true, pair: session?.pair === true, root: session?.root === true};
  }
  if (Number.isInteger(raw.lastSession) && raw.lastSession >= 1 && raw.lastSession <= 6) profile.lastSession = raw.lastSession;
  return profile;
}
export function blank(student) { return normalize({schema: 1, course: COURSE, student}); }
export function merge(left, right) {
  const a = normalize(left), b = normalize(right);
  if (a.student !== b.student) throw new Error('No se mezclan perfiles de estudiantes diferentes.');
  a.achievements = [...new Set([...a.achievements, ...b.achievements])];
  for (let i = 1; i <= 6; i++) for (const key of ['prepare', 'pair', 'root']) a.sessions[i][key] ||= b.sessions[i][key];
  return a;
}
export function load(storage) {
  const raw = storage.getItem(KEY);
  if (!raw) return {active: 'alumno', profiles: [blank('alumno')]};
  const parsed = JSON.parse(raw);
  if (!Array.isArray(parsed.profiles) || parsed.profiles.length < 1 || parsed.profiles.length > 100)
    throw new Error('El almacenamiento local no contiene perfiles válidos. Importa una copia de respaldo.');
  const profiles = parsed.profiles.map(normalize);
  if (new Set(profiles.map(p => p.student)).size !== profiles.length) throw new Error('Hay perfiles duplicados.');
  return {active: profiles.some(p => p.student === parsed.active) ? parsed.active : profiles[0].student, profiles};
}
export function save(storage, state) {
  storage.setItem(KEY, JSON.stringify({active: state.active, profiles: state.profiles.map(normalize)}));
}
