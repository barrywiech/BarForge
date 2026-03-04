// ─── State ────────────────────────────────────────────────────────────────────
let state   = { pumps: [], status: 'IDLE', estop: false, ip: '' };
let recipes = { recipes: [] };
let calTimer = null;

// ─── Navigation ───────────────────────────────────────────────────────────────
function showPage(id, btn) {
  document.querySelectorAll('.page').forEach(p => p.classList.remove('active'));
  document.querySelectorAll('nav button').forEach(b => b.classList.remove('active'));
  document.getElementById('page-' + id).classList.add('active');
  if (btn) btn.classList.add('active');

  if (id === 'recipes')   renderRecipes();
  if (id === 'setup')     renderSetup();
  if (id === 'calibrate') updateCalPumpLabels();
}

// ─── Toast ────────────────────────────────────────────────────────────────────
let toastTimer;
function toast(msg, type = 'ok') {
  const el = document.getElementById('toast');
  el.textContent = msg;
  el.className = 'show ' + type;
  clearTimeout(toastTimer);
  toastTimer = setTimeout(() => el.className = '', 2800);
}

// ─── API helpers ──────────────────────────────────────────────────────────────
async function api(method, path, body) {
  const opts = { method, headers: {} };
  if (body && typeof body === 'object' && !(body instanceof URLSearchParams)) {
    opts.headers['Content-Type'] = 'application/json';
    opts.body = JSON.stringify(body);
  } else if (body) {
    opts.body = body;
  }
  const res = await fetch(path, opts);
  return res.json();
}

// ─── Status polling ───────────────────────────────────────────────────────────
async function pollStatus() {
  try {
    const data = await api('GET', '/api/status');
    state = data;
    renderStatus();
  } catch (e) {
    // ignore network errors during polling
  }
}

function renderStatus() {
  const badge = document.getElementById('status-badge');
  badge.textContent = state.estop ? 'E-STOP' : state.status;
  badge.className = state.estop ? 'estop' : (state.status === 'DISPENSING' ? 'dispensing' : '');

  document.getElementById('ip-badge').textContent = state.ip || '';

  renderBottleList();
  renderQuickPour();
}

// ─── Bottle level rendering ───────────────────────────────────────────────────
function levelClass(pct) {
  if (pct > 0.5) return 'high';
  if (pct > 0.2) return 'medium';
  return 'low';
}

function renderBottleList() {
  const el = document.getElementById('bottle-list');
  if (!state.pumps) return;
  el.innerHTML = state.pumps.map(p => {
    const pct = p.bottleSize > 0 ? p.remaining / p.bottleSize : 0;
    const pctClamped = Math.min(1, Math.max(0, pct));
    return `
      <div class="bottle-row">
        <div class="bottle-label">
          <div class="bottle-name">P${p.id} ${p.spirit}</div>
          <div class="bottle-vol">${Math.round(p.remaining)} / ${Math.round(p.bottleSize)} ml</div>
        </div>
        <div class="bar-track">
          <div class="bar-fill ${levelClass(pct)}" style="width:${(pctClamped*100).toFixed(1)}%"></div>
        </div>
        <div class="bottle-pct">${(pctClamped*100).toFixed(0)}%</div>
        <div class="active-dot ${p.active ? 'on' : ''}"></div>
      </div>`;
  }).join('');
}

function renderQuickPour() {
  const el = document.getElementById('quick-pour-grid');
  if (!state.pumps) return;
  el.innerHTML = state.pumps.map(p => `
    <button class="btn btn-secondary" onclick="quickPour(${p.id}, 30)">
      P${p.id} ${p.spirit}<br><small>30 ml</small>
    </button>`).join('');
}

// ─── Pour actions ─────────────────────────────────────────────────────────────
async function quickPour(pump, ml) {
  const r = await api('POST', `/api/pour/manual?pump=${pump}&ml=${ml}`);
  toast(r.status || r.error, r.error ? 'err' : 'ok');
}

async function pourManual() {
  const pump = document.getElementById('manual-pump').value;
  const ml   = document.getElementById('manual-ml').value;
  const r = await api('POST', `/api/pour/manual?pump=${pump}&ml=${ml}`);
  toast(r.status || r.error, r.error ? 'err' : 'ok');
}

function setManualMl(ml) {
  document.getElementById('manual-ml').value = ml;
}

async function pourRecipe() {
  const name = document.getElementById('pour-recipe-select').value;
  if (!name) { toast('Select a recipe first', 'err'); return; }
  const r = await api('POST', '/api/pour/recipe', { name });
  toast(r.status || r.error, r.error ? 'err' : 'ok');
}

async function apiStop() {
  const r = await api('POST', '/api/stop');
  toast('All pumps stopped', 'ok');
}

// ─── Recipes page ─────────────────────────────────────────────────────────────
async function loadRecipes() {
  recipes = await api('GET', '/api/recipes');
  renderRecipeSelect();
}

function renderRecipeSelect() {
  const sel = document.getElementById('pour-recipe-select');
  sel.innerHTML = '<option value="">— choose —</option>' +
    recipes.recipes.map(r => `<option value="${r.name}">${r.name}</option>`).join('');
}

function renderRecipes() {
  const el = document.getElementById('recipe-list');
  if (!recipes.recipes.length) {
    el.innerHTML = '<p class="muted">No recipes yet.</p>';
    return;
  }
  el.innerHTML = recipes.recipes.map((r, i) => `
    <div class="recipe-item">
      <div>
        <div class="recipe-name">${r.name}</div>
        <div class="recipe-steps">${r.steps.map(s => `P${s.pump} ${s.ml}ml`).join(' + ')}</div>
      </div>
      <button class="btn btn-secondary btn-sm" onclick="editRecipe(${i})">Edit</button>
      <button class="btn btn-danger btn-sm" onclick="deleteRecipe(${i})">Del</button>
    </div>`).join('');
}

let recipeStepCount = 0;
function addRecipeStep(pump = 1, ml = 30) {
  const i = recipeStepCount++;
  const el = document.getElementById('recipe-steps-editor');
  const row = document.createElement('div');
  row.className = 'form-cols';
  row.id = 'step-row-' + i;
  row.style.marginBottom = '8px';
  const pumpOpts = [1,2,3,4,5].map(n =>
    `<option value="${n}" ${n===pump?'selected':''}>${
      (state.pumps[n-1] || {spirit:'Pump '+n}).spirit || 'Pump '+n}</option>`
  ).join('');
  row.innerHTML = `
    <div class="form-row">
      <label>Pump</label>
      <select id="step-pump-${i}">${pumpOpts}</select>
    </div>
    <div class="form-row" style="display:flex;gap:8px;align-items:flex-end">
      <div style="flex:1">
        <label>Volume (ml)</label>
        <input type="number" id="step-ml-${i}" value="${ml}" min="1" max="200">
      </div>
      <button class="btn btn-danger btn-sm" onclick="document.getElementById('step-row-${i}').remove()">✕</button>
    </div>`;
  el.appendChild(row);
}

function editRecipe(idx) {
  const r = recipes.recipes[idx];
  document.getElementById('new-recipe-name').value = r.name;
  document.getElementById('recipe-steps-editor').innerHTML = '';
  recipeStepCount = 0;
  r.steps.forEach(s => addRecipeStep(s.pump, s.ml));
  deleteRecipe(idx, true);
}

function deleteRecipe(idx, silent = false) {
  recipes.recipes.splice(idx, 1);
  if (!silent) {
    api('POST', '/api/recipes', recipes).then(() => {
      toast('Recipe deleted', 'ok');
      renderRecipes();
      renderRecipeSelect();
    });
  }
}

async function saveRecipe() {
  const name = document.getElementById('new-recipe-name').value.trim();
  if (!name) { toast('Enter a recipe name', 'err'); return; }

  const steps = [];
  document.querySelectorAll('[id^="step-pump-"]').forEach(sel => {
    const i  = sel.id.replace('step-pump-', '');
    const ml = parseFloat(document.getElementById('step-ml-' + i).value);
    if (!isNaN(ml) && ml > 0) {
      steps.push({ pump: parseInt(sel.value), ml });
    }
  });
  if (!steps.length) { toast('Add at least one step', 'err'); return; }

  recipes.recipes.push({ name, steps });
  const r = await api('POST', '/api/recipes', recipes);
  toast(r.status || r.error, r.error ? 'err' : 'ok');
  document.getElementById('new-recipe-name').value = '';
  document.getElementById('recipe-steps-editor').innerHTML = '';
  recipeStepCount = 0;
  renderRecipes();
  renderRecipeSelect();
}

// ─── Setup page ───────────────────────────────────────────────────────────────
function renderSetup() {
  const el = document.getElementById('setup-pumps');
  if (!state.pumps) return;
  el.innerHTML = state.pumps.map(p => `
    <div class="card" style="margin-bottom:12px">
      <h3>Pump ${p.id}</h3>
      <div class="form-cols">
        <div class="form-row">
          <label>Spirit Name</label>
          <input type="text" id="setup-spirit-${p.id}" value="${p.spirit}">
        </div>
        <div class="form-row">
          <label>Bottle Size (ml)</label>
          <input type="number" id="setup-size-${p.id}" value="${p.bottleSize}" min="1">
        </div>
      </div>
      <div class="form-cols">
        <div class="form-row">
          <label>Remaining (ml)</label>
          <input type="number" id="setup-remaining-${p.id}" value="${Math.round(p.remaining)}" min="0">
        </div>
        <div class="form-row">
          <label>ml/sec (calibrated)</label>
          <input type="number" id="setup-mlsec-${p.id}" value="${p.mlPerSec.toFixed(3)}" step="0.001" min="0.1">
        </div>
      </div>
    </div>`).join('');
}

async function saveConfig() {
  if (!state.pumps) return;
  const pumps = state.pumps.map(p => ({
    id:         p.id,
    spirit:     document.getElementById(`setup-spirit-${p.id}`).value,
    bottleSize: parseFloat(document.getElementById(`setup-size-${p.id}`).value),
    remaining:  parseFloat(document.getElementById(`setup-remaining-${p.id}`).value),
    mlPerSec:   parseFloat(document.getElementById(`setup-mlsec-${p.id}`).value),
  }));
  const r = await api('POST', '/api/config', { pumps });
  toast(r.status || r.error, r.error ? 'err' : 'ok');
  await pollStatus();
}

// ─── Calibration page ─────────────────────────────────────────────────────────
function updateCalPumpLabels() {
  // nothing extra needed — select is static
}

async function calStart() {
  const pump = document.getElementById('cal-pump-select').value;
  const r = await api('POST', `/api/calibrate/start?pump=${pump}`);
  if (r.error) { toast(r.error, 'err'); return; }

  toast('Calibration started — collect output!', 'ok');
  const SECS = 5;
  let elapsed = 0;
  const prog  = document.getElementById('cal-progress');
  const count = document.getElementById('cal-countdown');
  clearInterval(calTimer);
  calTimer = setInterval(() => {
    elapsed++;
    const pct = (elapsed / SECS) * 100;
    prog.style.width = Math.min(pct, 100) + '%';
    count.textContent = elapsed < SECS ? `${SECS - elapsed}s remaining` : 'Done — measure output now';
    if (elapsed >= SECS) clearInterval(calTimer);
  }, 1000);
}

async function calSave() {
  const pump = document.getElementById('cal-pump-select').value;
  const ml   = document.getElementById('cal-ml').value;
  if (!ml) { toast('Enter measured volume', 'err'); return; }

  const r = await api('POST', `/api/calibrate/save?pump=${pump}&ml=${ml}`);
  if (r.error) { toast(r.error, 'err'); return; }

  const mlPerSec = parseFloat(ml) / 5.0;
  document.getElementById('cal-result').textContent =
    `Pump ${pump} calibrated: ${mlPerSec.toFixed(3)} ml/s`;
  document.getElementById('cal-ml').value = '';
  document.getElementById('cal-progress').style.width = '0%';
  document.getElementById('cal-countdown').textContent = '';
  toast('Calibration saved!', 'ok');
  await pollStatus();
}

// ─── Bootstrap ────────────────────────────────────────────────────────────────
(async function init() {
  await pollStatus();
  await loadRecipes();
  setInterval(pollStatus, 2000);  // poll every 2 s
})();
