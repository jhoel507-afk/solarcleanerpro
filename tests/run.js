// Pruebas automáticas de la aplicación (sin navegador). Uso: node tests/run.js
const fs = require('fs'), path = require('path'), zlib = require('zlib'), crypto = require('crypto');
const raiz = path.join(__dirname, '..');
const html = fs.readFileSync(path.join(raiz, 'www', 'index.html'), 'utf8');
const scripts = [...html.matchAll(/<script>([\s\S]*?)<\/script>/g)].map(m => m[1]);
let pass = 0, fail = 0;
const ok = (c, m) => { if (c) pass++; else { fail++; console.log('  FALLA: ' + m); } };

function cargar() {
  global.C = undefined;
  const els = {};
  const El = id => { const e = { id, _h: '', textContent: '', className: '', style: {}, dataset: {}, value: '', disabled: false, hidden: false, selectedIndex: 1, children: [],
    setAttribute() {}, getAttribute() { return null }, addEventListener() {}, appendChild() {}, focus() {}, select() {}, classList: { toggle() {} } };
    Object.defineProperty(e, 'innerHTML', { get() { return e._h }, set(v) { e._h = v; if (id === 'dirt' || id === 'drops') e.children = Array.from({ length: (v.match(/<(rect|circle)/g) || []).length }, () => El('c')); if (id === 'dlock' || id === 'dpin') for (const k of ['dl_a', 'dl_b', 'dl_ok', 'dl_skip', 'dl_forgot', 'dl_m', 'dp_a', 'dp_set', 'dp_del', 'dp_m']) delete els[k]; } });
    return e; };
  const layout = { classList: { toggle() {} } };
  global.document = { getElementById: id => els[id] || (els[id] = El(id)), querySelectorAll: () => [], querySelector: q => q === '.layout' ? layout : null, body: { style: {} } };
  global.innerWidth = 1000; global.matchMedia = () => ({ matches: false }); global.scrollTo = () => {}; global.requestAnimationFrame = () => {};
  global.localStorage = { _d: {}, getItem(k) { return this._d[k] || null }, setItem(k, v) { this._d[k] = v }, removeItem(k) { delete this._d[k] } };
  global.window = global; global.addEventListener = () => {}; global.location = { protocol: 'file:', hostname: '', host: '', reload() {} };
  const api = new Function(scripts[0] + ';\n' + scripts[1] + ';return {C,DEF,LIVE,origRoute,exportCode,get PFMT(){return PFMT},get BOM(){return BOM},get T42(){return T42},S,pg,sha256,recalc,compute,informe,setPanels,buildPlan,bomTot,tTot,draw,get NB(){return NB}};')();
  window.C = api.C;
  return { api, els };
}

console.log('1. Resumen criptográfico del PIN');
{ const { api } = cargar(); for (const t of ['', 'abc', '1234', 'SolarCleanerPro|1234']) ok(api.sha256(t) === crypto.createHash('sha256').update(t).digest('hex'), 'SHA-256 de "' + t + '"'); }

console.log('2. Cálculos con los datos por defecto');
{ const { api } = cargar(); const R = api.compute(api.C);
  ok(R.nN === 3, 'boquillas fijas: una por panel (3)'); ok(R.Q > 0 && R.Q < R.Q0, 'caudal entre 0 y el máximo de la bomba');
  ok(R.P > 0 && R.P < 0.5, 'presión en boquilla menor que la presión máxima de la bomba'); ok(R.T > 0 && R.Tp > 0, 'tiempos de ciclo');
  ok(Math.abs(R.rail - 3.43) < 0.01, 'recorrido de 3 paneles = 3,43 m'); ok(R.busW === 204, 'salida de 12 V = 204 W');
  ok(R.IB <= R.fuse && R.fuse <= R.IZ, 'fusible cumple IB ≤ In ≤ IZ'); ok(R.main.b > 1, 'se detecta que el bloqueo supera la fuente'); }

console.log('3. Paneles y disposición de boquillas');
{ const { api } = cargar(); api.setPanels(4); let R = api.compute(api.C); ok(Math.abs(R.rail - 4.58) < 0.01 && R.nN === 4, '4 paneles: 4,58 m y 4 boquillas'); ok(api.NB === 86, 'dibujo de 4 paneles');
  api.C.nozMode = 1; R = api.compute(api.C); ok(R.nN === 1, 'boquilla en el carro: 1'); api.C.nozMode = 0; api.setPanels(3); ok(api.NB === 64, 'vuelta a 3 paneles'); }

console.log('4. Costos: materiales + mano de obra = inversión');
{ const { api } = cargar(); ok(api.compute(api.C).inv === 1200, 'sin precios usa la inversión supuesta');
  api.BOM[0].p = 100; api.BOM[0].q = 2; api.T42[0].h = 10; api.C.moHora = 7; api.bomTot(); api.tTot();
  const R = api.compute(api.C); ok(R.inv === 270, 'inversión = 200 + 70 (' + R.inv + ')'); ok(R.sens.length === 4, 'escenarios de pérdida por suciedad'); }

console.log('5. Textos del informe');
{ const { api } = cargar(); for (const [k, h] of [['dim', '3.2.3'], ['sel', '3.2.4'], ['esp', '3.2.10'], ['mat', '4.1'], ['tim', '4.2'], ['cost', '4.3.1'], ['roi', '4.5']]) { const t = api.informe(k); ok(t.includes(h) && t.length > 200, 'sección ' + h); }
  ok(api.informe('mat').includes('PRODUCTO | MARCA | UNIDAD | COSTO UNITARIO | CANTIDAD | TOTAL'), 'tabla 4.1 con las columnas de la guía'); ok(api.informe('esp').includes('Bloque funcional | Parámetro | Valor final alcanzado'), 'tabla 3.2.10 por bloques'); }

console.log('6. Valores extremos y aleatorios (sin NaN ni undefined)');
{ const { api, els } = cargar(); const mala = /NaN|undefined|Infinity|\[object/; let malos = 0;
  const revisar = () => { api.recalc(); for (const id of ['glob', 'tiles', 'verd', 'rw', 're', 'rd', 'rrn', 'rl', 'dsum']) if (mala.test((els[id] && els[id].innerHTML) || '')) malos++; for (const k of ['dim', 'sel', 'esp', 'mat', 'tim', 'cost', 'roi']) if (mala.test(api.informe(k))) malos++; };
  const extremos = [{ vel: 0.0001, mRun: 0, pumpA: 0, force: 0, nozQ: 0.001, tarifa: 0, pw: 0.01, cabS: 0.5 }, { hs: 50 }, { pumpH: 0.5, pumpQ: 10 }, { np: 4, nozMode: 1 }, { cleansMonth: 0, hrsClean: 0, costHour: 0 }, { cabL: 50, cabS: 1.5 }];
  revisar(); for (const v of extremos) { Object.assign(api.C, JSON.parse(JSON.stringify(api.DEF)), v); revisar(); }
  let seed = 7; const rnd = () => (seed = seed * 16807 % 2147483647) / 2147483647;
  for (let i = 0; i < 300; i++) { for (const k of Object.keys(api.DEF)) { const d = api.DEF[k]; if (typeof d !== 'number') continue; api.C[k] = k === 'np' ? (rnd() < .5 ? 3 : 4) : (k === 'nozMode' || k === 'bombaRec') ? (rnd() < .5 ? 0 : 1) : +(d * (0.2 + rnd() * 3)).toFixed(3); } revisar(); }
  ok(malos === 0, 'sin valores inválidos en la interfaz (' + malos + ' hallazgos)'); }

console.log('7. Hoja de datos con PIN');
{ const { api, els } = cargar(); const g = n => document.getElementById(n);
  api.pg(5); ok(g('dbody').hidden === true, 'la hoja de datos arranca bloqueada');
  g('dl_a').value = '12'; g('dl_b').value = '12'; g('dl_ok').onclick(); ok(g('dbody').hidden === true, 'rechaza un PIN corto');
  g('dl_a').value = '1234'; g('dl_b').value = '1234'; g('dl_ok').onclick(); ok(g('dbody').hidden === false, 'crea el PIN y abre');
  ok(!JSON.stringify(JSON.parse(localStorage.getItem('scp_pin'))).includes('1234'), 'no guarda el PIN en claro');
  api.pg(3); api.pg(5); ok(g('dbody').hidden === true, 'se bloquea al salir');
  g('dl_a').value = '0000'; g('dl_ok').onclick(); ok(g('dbody').hidden === true, 'PIN incorrecto no abre');
  g('dl_a').value = '1234'; g('dl_ok').onclick(); ok(g('dbody').hidden === false, 'PIN correcto abre');
  api.BOM[0].p = 77; g('bkc').onclick(); const copia = g('bkt').value; api.BOM[0].p = 0; g('bkt').value = copia; g('bka').onclick(); ok(api.BOM[0].p === 77, 'copia de seguridad restaura los precios'); }

console.log('8. Compatibilidad con el programa original (rutas y formato del código)');
{ const { api } = cargar(); const S = api.S, R = api.origRoute;
  S.run = 0; S.dir = 0; ok(R('bs').p === '/auto', 'Iniciar ciclo usa /auto'); ok(R('bx').p === '/stop', 'Detener usa /stop'); ok(R('k1').p === '/bomba', 'Bomba usa /bomba'); ok(R('es').p === '/motor', 'Escobilla usa /motor');
  ok(!!R('k2').m && !R('k2').p, 'K2 manual no existe en el programa original'); S.run = 1; ok(!!R('bs').m, 'no reinicia un ciclo en marcha'); ok(!!R('k1').m, 'la bomba manual exige sistema en espera'); ok(R('bx').p === '/stop', 'Detener siempre disponible');
  ok(api.C.tPausa === 0.5 && api.C.pwm === 180 && api.C.tFin === 0, 'valores por defecto iguales al programa original');
  const cod = api.exportCode(); ok(api.PFMT === 'orig' && /const unsigned long TIEMPO_PRELAVADO = 5000;/.test(cod) && /const int VELOCIDAD_MOTOR = 180;/.test(cod) && /int vueltasConfiguradas = 3;/.test(cod), 'exporta con los nombres del programa original');
  document.getElementById('pitx').value = 'const unsigned long TIEMPO_PRELAVADO = 8000;\nconst unsigned long PAUSA_CAMBIO_GIRO = 700;\nconst int VELOCIDAD_MOTOR = 150;\nint vueltasConfiguradas = 2;'; document.getElementById('pap').onclick();
  ok(api.C.tPre === 8 && api.C.tPausa === 0.7 && api.C.pwm === 150 && api.C.laps === 2, 'importa desde el código original (ms a s)'); }

console.log('9. La página del ESP32 está actualizada');
{ const h = fs.readFileSync(path.join(raiz, 'esp32', 'SolarCleanerPro_ESP32', 'pagina_web.h'), 'utf8');
  const bytes = Buffer.from([...h.matchAll(/0x([0-9a-f]{2})/g)].map(m => parseInt(m[1], 16)));
  ok(zlib.gunzipSync(bytes).toString('utf8') === html, 'pagina_web.h coincide con www/index.html (si falla: python3 tools/generar_pagina.py)'); }

console.log('\n' + pass + ' comprobaciones correctas, ' + fail + ' fallidas');
process.exit(fail ? 1 : 0);
