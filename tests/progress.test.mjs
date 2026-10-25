import test from 'node:test';
import assert from 'node:assert/strict';
import {KEY, blank, normalize, merge, load, save} from '../docs/progress.mjs';

test('perfiles aislados y persistentes después de recargar', () => {
  const memory = new Map(), storage = {getItem: k => memory.get(k), setItem: (k,v) => memory.set(k,v)};
  const state = load(storage);
  state.profiles[0].sessions[1].pair = true;
  state.profiles.push(blank('bea')); state.active = 'bea'; save(storage, state);
  const restored = load(storage);
  assert.equal(restored.active, 'bea'); assert.equal(restored.profiles[0].sessions[1].pair, true);
  assert.equal(restored.profiles[1].sessions[1].pair, false);
  assert.ok(memory.has(KEY));
});
test('importar progreso nativo combina avances del mismo estudiante', () => {
  const web = blank('ana'); web.sessions[1].root = true;
  const native = {schema:1,course:'cpp-root-course',student:'ana',demo:false,achievements:['check01','first_move'],sessions:{1:{pair:true}}};
  const result = merge(web, native);
  assert.equal(result.sessions[1].root, true); assert.equal(result.sessions[1].pair, true);
  assert.deepEqual(result.achievements, ['check01','first_move']);
  assert.equal(web.sessions[1].pair, false);
});
test('rechaza demo, otra asignatura y mezcla de estudiantes', () => {
  assert.throws(() => normalize({...blank('ana'), demo:true}));
  assert.throws(() => normalize({...blank('ana'), course:'another'}));
  assert.throws(() => merge(blank('ana'), blank('bea')));
});
test('valida datos importados y solo acepta booleanos y logros conocidos', () => {
  const profile = normalize({...blank('ana'), achievements:['check01','check01','inventado','toString'],sessions:{1:{pair:'false',root:true}}});
  assert.deepEqual(profile.achievements,['check01']); assert.equal(profile.sessions[1].pair,false); assert.equal(profile.sessions[1].root,true);
  for (const student of ['../ana','<script>','a b','a'.repeat(33)]) assert.throws(() => blank(student));
});
test('no oculta almacenamiento corrupto o bloqueado', () => {
  assert.throws(() => load({getItem: () => '{bad'}));
  assert.throws(() => save({setItem: () => {throw Error('quota');}}, {active:'ana',profiles:[blank('ana')]}));
});
