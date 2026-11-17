#pragma once

#include <Arduino.h>

const char INDEX_HTML[] PROGMEM = R"RAWHTML(
<!DOCTYPE html>
<html lang="en">
<head>
<meta charset="utf-8">
<meta name="viewport" content="width=device-width, initial-scale=1">
<title>NFC Player</title>
<style>
  :root { --bg:#111318; --panel:#1a1e26; --line:#2b3240; --text:#e8edf5; --muted:#8a97a8; --accent:#5cc8ff; --danger:#ff6b6b; --ok:#6ee7a8; }
  * { box-sizing: border-box; }
  body { margin:0; font-family: system-ui, -apple-system, Segoe UI, Roboto, sans-serif; background:var(--bg); color:var(--text); }
  header { padding:16px 18px 8px; }
  h1 { margin:0; font-size:22px; letter-spacing:.2px; }
  h2 { margin:0 0 10px; font-size:16px; }
  .muted { color:var(--muted); }
  nav { display:flex; gap:6px; padding:8px 12px; overflow-x:auto; border-bottom:1px solid var(--line); }
  nav button { background:transparent; color:var(--muted); border:1px solid transparent; padding:8px 12px; border-radius:8px; cursor:pointer; font-size:14px; }
  nav button.active { color:var(--text); background:var(--panel); border-color:var(--line); }
  main { padding:14px; max-width:900px; margin:0 auto; }
  .tab-panel { display:none; }
  .tab-panel.active { display:block; }
  .card { background:var(--panel); border:1px solid var(--line); border-radius:12px; padding:14px; margin-bottom:12px; }
  .card.inset { background:#151922; border-style:dashed; }
  .row { display:flex; flex-wrap:wrap; gap:10px; align-items:center; }
  label { display:block; margin:10px 0 4px; color:var(--muted); font-size:13px; }
  input[type=text], input[type=password], input[type=number], select { width:100%; padding:10px; border-radius:8px; border:1px solid var(--line); background:#0e1116; color:var(--text); font-size:15px; }
  input[type=range] { width:180px; vertical-align:middle; }
  button { background:var(--accent); color:#06121a; border:0; border-radius:8px; padding:10px 14px; font-weight:700; cursor:pointer; font-size:14px; }
  button.secondary { background:var(--line); color:var(--text); }
  button.danger { background:var(--danger); color:#1a0505; }
  button:disabled { opacity:.5; cursor:not-allowed; }
  table { width:100%; border-collapse:collapse; font-size:14px; }
  th, td { text-align:left; padding:8px 6px; border-bottom:1px solid var(--line); vertical-align:middle; }
  th { color:var(--muted); font-weight:600; font-size:12px; text-transform:uppercase; letter-spacing:.5px; }
  code { color:var(--accent); }
  .pill { display:inline-block; padding:2px 8px; border-radius:999px; background:var(--line); font-size:12px; color:var(--text); }
  .pill.playing { background:#12352a; color:var(--ok); }
  .pill.learning { background:#3a2a12; color:#ffd48a; }
  progress { width:100%; height:10px; margin-top:10px; }
  pre { white-space:pre-wrap; margin:0; color:var(--muted); font-size:12px; }
  .small { font-size:12px; }
  .file-actions button { padding:6px 10px; font-size:12px; margin-left:6px; }
  @media (max-width: 560px) { th:nth-child(2), td:nth-child(2) { display:none; } }
</style>
</head>
<body>
<header>
  <h1>NFC Player</h1>
  <div id="netline" class="muted small">connecting...</div>
</header>
<nav>
  <button class="active" data-tab="play">Play</button>
  <button data-tab="files">Files</button>
  <button data-tab="cards">Cards</button>
  <button data-tab="settings">Settings</button>
</nav>
<main>
  <section id="tab-play" class="tab-panel active">
    <div class="card">
      <div class="row" style="justify-content:space-between">
        <strong id="nowFile">Idle</strong>
        <span id="nowState" class="pill">idle</span>
      </div>
      <p id="nowMeta" class="muted small"></p>
      <div class="row" style="margin-top:12px">
        <button id="stopBtn" class="danger">Stop</button>
        <label style="margin:0">Volume <input id="volume" type="range" min="0" max="21" value="15"></label>
        <span id="volLabel" class="muted">15</span>
      </div>
    </div>
    <div class="card">
      <h2>How it works</h2>
      <ul class="muted small">
        <li>Tap a learned card to play its one song.</li>
        <li>Tap the same card again to stop; tap another card to switch.</li>
        <li>Lift the card and the song keeps playing to the end.</li>
        <li>Stop button stops immediately; hold it 3 s to sleep.</li>
        <li>Upload audio here, then assign it to a card under Cards.</li>
      </ul>
    </div>
  </section>

  <section id="tab-files" class="tab-panel">
    <div class="card">
      <h2>Upload audio to SD</h2>
      <form id="uploadForm">
        <input id="fileInput" type="file" accept=".mp3,.m4a,.aac,.flac,.ogg,.opus,.wav">
        <div class="row" style="margin-top:10px"><button type="submit" id="uploadBtn">Upload</button></div>
      </form>
      <progress id="uploadProgress" value="0" max="100" hidden></progress>
      <p id="uploadMsg" class="muted small"></p>
      <p class="muted small">Supported: MP3, M4A/AAC, FLAC, OGG/Opus, WAV. Playback stops during upload. Files are written to the SD card.</p>
    </div>
    <div class="card">
      <h2>Files <span id="fileCount" class="muted small"></span></h2>
      <table>
        <thead><tr><th>Name</th><th>Size</th><th></th></tr></thead>
        <tbody id="fileRows"></tbody>
      </table>
      <p id="fileEmpty" class="muted small" hidden>No audio files yet.</p>
    </div>
  </section>

  <section id="tab-cards" class="tab-panel">
    <div class="card">
      <h2>Learn a card</h2>
      <p class="muted small">One card plays one song. Ask the reader to learn the next tap, then assign a file.</p>
      <button id="learnBtn">Learn new card</button>
      <div id="learnBox" class="card inset" hidden>
        <p id="learnMsg" class="muted">Tap a card now (60 s)...</p>
        <div id="learnForm" hidden>
          <p>UID: <code id="learnUid"></code></p>
          <label>Track <select id="learnFile"></select></label>
          <label>Label <input id="learnLabel" type="text" maxlength="48" placeholder="Bedtime song"></label>
          <div class="row">
            <button id="learnSave">Save card</button>
            <button id="learnCancel" class="secondary">Cancel</button>
          </div>
        </div>
      </div>
    </div>
    <div class="card">
      <h2>Known cards</h2>
      <table>
        <thead><tr><th>Card</th><th>File</th><th></th></tr></thead>
        <tbody id="cardRows"></tbody>
      </table>
      <p id="cardEmpty" class="muted small" hidden>No cards learned yet.</p>
    </div>
  </section>

  <section id="tab-settings" class="tab-panel">
    <div class="card">
      <h2>Device</h2>
      <label>Device name (mDNS)</label>
      <input id="devName" type="text" maxlength="24" placeholder="nfcplayer">
      <label>Default volume <span id="setVolLabel" class="muted"></span></label>
      <input id="setVolume" type="range" min="0" max="21" value="15">
      <label>AP password (at least 8 characters)</label>
      <input id="apPass" type="password" minlength="8">
      <label>Web login password (leave empty to keep current)</label>
      <input id="webPass" type="password" placeholder="optional">
      <label class="small"><input type="checkbox" id="webClear" style="width:auto"> Remove web login</label>
      <div class="row" style="margin-top:12px"><button id="saveSettings">Save settings</button></div>
      <p id="settingsMsg" class="muted small"></p>
    </div>
    <div class="card">
      <h2>Wi-Fi</h2>
      <p id="wifiNow" class="muted small"></p>
      <label>Network name</label>
      <input id="wifiSsid" type="text" placeholder="Home Wi-Fi">
      <label>Password</label>
      <input id="wifiPass" type="password">
      <div class="row" style="margin-top:10px">
        <button id="scanBtn" class="secondary">Scan</button>
        <select id="wifiList"><option value="">Scanned networks...</option></select>
      </div>
      <div class="row" style="margin-top:10px"><button id="saveWifi">Save &amp; reboot</button></div>
      <p id="wifiMsg" class="muted small"></p>
    </div>
    <div class="card">
      <h2>System</h2>
      <pre id="systemInfo">loading...</pre>
      <div class="row" style="margin-top:10px"><button id="rebootBtn" class="danger">Reboot</button></div>
    </div>
  </section>
</main>

<script>
const $ = (s) => document.querySelector(s);
let files = [];
let cards = [];
let learnTimer = null;

function esc(s) {
  return String(s).replace(/[&<>"']/g, (c) => ({'&':'&amp;','<':'&lt;','>':'&gt;','"':'&quot;',"'":'&#39;'}[c]));
}

function fmtBytes(n) {
  if (!n) return '0 B';
  const u = ['B','KB','MB','GB'];
  let i = 0;
  while (n >= 1024 && i < u.length - 1) { n /= 1024; i++; }
  return (i === 0 ? n : n.toFixed(1)) + ' ' + u[i];
}

async function api(path, opts) {
  const r = await fetch(path, Object.assign({cache:'no-store'}, opts || {}));
  if (r.status === 401) { window.location.reload(); throw new Error('login required'); }
  const text = await r.text();
  let data = {};
  try { data = text ? JSON.parse(text) : {}; } catch (e) { data = {error:text}; }
  if (!r.ok) throw new Error(data.error || text || ('HTTP ' + r.status));
  return data;
}

function setTab(name) {
  document.querySelectorAll('nav button').forEach(b => b.classList.toggle('active', b.dataset.tab === name));
  document.querySelectorAll('.tab-panel').forEach(p => p.classList.toggle('active', p.id === 'tab-' + name));
  if (name === 'files') refreshFiles();
  if (name === 'cards') refreshCards();
  if (name === 'settings') refreshSettings();
  if (name === 'play') refreshStatus();
}

document.querySelectorAll('nav button').forEach(b => b.addEventListener('click', () => setTab(b.dataset.tab)));

async function refreshStatus() {
  try {
    const s = await api('/api/status');
    $('#nowState').textContent = s.state || 'idle';
    $('#nowState').className = 'pill ' + (s.state || 'idle');
    $('#nowFile').textContent = s.file ? s.file : (s.state === 'playing' ? 'Playing...' : 'Idle');
    let meta = [];
    if (s.state === 'playing') meta.push('card ' + (s.card || 'web'));
    if (s.duration) meta.push(Math.floor(s.position || 0) + 's / ' + s.duration + 's');
    if (s.ap) meta.push('AP mode');
    $('#nowMeta').textContent = meta.join(' · ');
    $('#volume').value = s.volume;
    $('#volLabel').textContent = s.volume;
    $('#netline').textContent = s.ap
      ? ('AP ' + s.ap_ssid + ' — http://' + s.ip)
      : ('Wi-Fi ' + (s.ip || 'connecting') + (s.rssi ? ' (' + s.rssi + ' dBm)' : ''));
    $('#systemInfo').textContent =
      'Firmware: ' + s.version + '\n' +
      'State: ' + s.state + '\n' +
      'IP: ' + s.ip + '\n' +
      'Wi-Fi RSSI: ' + s.rssi + ' dBm\n' +
      'SD: ' + (s.sd ? 'ready' : 'not ready') + '\n' +
      'Free: ' + fmtBytes(s.free) + ' / ' + fmtBytes(s.total) + '\n' +
      'NFC: ' + (s.nfc ? 'ready' : 'not ready') + '\n' +
      'Uptime: ' + s.uptime + ' s';
  } catch (e) {
    $('#netline').textContent = 'offline: ' + e.message;
  }
}

function renderFiles() {
  const body = $('#fileRows');
  body.innerHTML = '';
  $('#fileCount').textContent = files.length ? '(' + files.length + ')' : '';
  $('#fileEmpty').hidden = files.length > 0;
  files.forEach(f => {
    const tr = document.createElement('tr');
    tr.innerHTML = '<td>' + esc(f.name) + '</td><td>' + fmtBytes(f.size) + '</td>' +
      '<td class="file-actions"><button data-play="' + esc(f.name) + '">Play</button>' +
      '<button class="danger" data-del="' + esc(f.name) + '">Delete</button></td>';
    body.appendChild(tr);
  });
  body.querySelectorAll('[data-play]').forEach(b => b.addEventListener('click', () => playFile(b.dataset.play)));
  body.querySelectorAll('[data-del]').forEach(b => b.addEventListener('click', () => deleteFile(b.dataset.del)));
}

async function refreshFiles() {
  try {
    const d = await api('/api/files');
    files = d.files || [];
    renderFiles();
    fillLearnFiles();
  } catch (e) {
    $('#uploadMsg').textContent = 'Cannot list files: ' + e.message;
  }
}

function fillLearnFiles() {
  const sel = $('#learnFile');
  const current = sel.value;
  sel.innerHTML = '';
  files.forEach(f => {
    const o = document.createElement('option');
    o.value = f.name;
    o.textContent = f.name;
    sel.appendChild(o);
  });
  if (current) sel.value = current;
}

async function playFile(name) {
  try { await api('/api/play?name=' + encodeURIComponent(name), {method:'POST'}); refreshStatus(); }
  catch (e) { alert('Play failed: ' + e.message); }
}

async function deleteFile(name) {
  if (!confirm('Delete ' + name + ' from the SD card?')) return;
  try {
    await api('/api/delete?name=' + encodeURIComponent(name), {method:'POST'});
    await refreshFiles();
    await refreshCards();
  } catch (e) { alert('Delete failed: ' + e.message); }
}

$('#stopBtn').addEventListener('click', async () => {
  try { await api('/api/stop', {method:'POST'}); refreshStatus(); } catch (e) { alert(e.message); }
});

$('#volume').addEventListener('change', async () => {
  const v = $('#volume').value;
  $('#volLabel').textContent = v;
  try { await api('/api/volume?level=' + v, {method:'POST'}); } catch (e) { alert(e.message); }
});

$('#uploadForm').addEventListener('submit', (ev) => {
  ev.preventDefault();
  const file = $('#fileInput').files[0];
  if (!file) { $('#uploadMsg').textContent = 'Choose a file first.'; return; }

  const fd = new FormData();
  fd.append('file', file, file.name);

  const xhr = new XMLHttpRequest();
  xhr.open('POST', '/api/upload');
  $('#uploadBtn').disabled = true;
  $('#uploadProgress').hidden = false;
  $('#uploadProgress').value = 0;
  $('#uploadMsg').textContent = 'Uploading ' + file.name + '...';

  xhr.upload.onprogress = (e) => {
    if (e.lengthComputable) $('#uploadProgress').value = Math.round((e.loaded / e.total) * 100);
  };
  xhr.onload = async () => {
    $('#uploadBtn').disabled = false;
    if (xhr.status === 401) { window.location.reload(); return; }
    let data = {};
    try { data = JSON.parse(xhr.responseText); } catch (e) {}
    if (xhr.status === 200 && data.ok) {
      $('#uploadProgress').value = 100;
      $('#uploadMsg').textContent = 'Saved as ' + data.name + ' (' + fmtBytes(data.size) + ').';
      $('#fileInput').value = '';
      await refreshFiles();
    } else {
      $('#uploadMsg').textContent = 'Upload failed: ' + (data.error || ('HTTP ' + xhr.status));
    }
  };
  xhr.onerror = () => {
    $('#uploadBtn').disabled = false;
    $('#uploadMsg').textContent = 'Upload failed: network error.';
  };
  xhr.send(fd);
});

async function refreshCards() {
  try {
    const d = await api('/api/cards');
    cards = d.cards || [];
    const body = $('#cardRows');
    body.innerHTML = '';
    $('#cardEmpty').hidden = cards.length > 0;
    cards.forEach(c => {
      const tr = document.createElement('tr');
      const exists = c.exists ? '' : ' <span class="muted">(missing file)</span>';
      tr.innerHTML = '<td><code>' + esc(c.uid) + '</code><br><span class="muted small">' + esc(c.label) + '</span></td>' +
        '<td>' + esc(c.file) + exists + '</td>' +
        '<td class="file-actions"><button class="danger" data-uid="' + esc(c.uid) + '">Delete</button></td>';
      body.appendChild(tr);
    });
    body.querySelectorAll('[data-uid]').forEach(b => b.addEventListener('click', async () => {
      if (!confirm('Delete this card mapping?')) return;
      try { await api('/api/cards/delete?uid=' + encodeURIComponent(b.dataset.uid), {method:'POST'}); await refreshCards(); }
      catch (e) { alert(e.message); }
    }));
  } catch (e) {
    $('#cardEmpty').hidden = false;
    $('#cardEmpty').textContent = 'Cannot load cards: ' + e.message;
  }
}

$('#learnBtn').addEventListener('click', async () => {
  try {
    await api('/api/learn/start', {method:'POST'});
    $('#learnBox').hidden = false;
    $('#learnForm').hidden = true;
    $('#learnUid').textContent = '';
    $('#learnMsg').textContent = 'Tap a card now (60 s)...';
    clearInterval(learnTimer);
    learnTimer = setInterval(async () => {
      try {
        const s = await api('/api/learn');
        if (!s.active) {
          clearInterval(learnTimer);
          $('#learnMsg').textContent = 'Learn window ended. Try again.';
          return;
        }
        if (s.uid) {
          clearInterval(learnTimer);
          $('#learnUid').textContent = s.uid;
          $('#learnForm').hidden = false;
          $('#learnMsg').textContent = 'Card captured. Choose its song.';
          fillLearnFiles();
        }
      } catch (e) {}
    }, 800);
  } catch (e) { alert('Could not start learn mode: ' + e.message); }
});

$('#learnCancel').addEventListener('click', async () => {
  clearInterval(learnTimer);
  try { await api('/api/learn/cancel', {method:'POST'}); } catch (e) {}
  $('#learnBox').hidden = true;
});

$('#learnSave').addEventListener('click', async () => {
  const uid = $('#learnUid').textContent;
  const file = $('#learnFile').value;
  const label = $('#learnLabel').value;
  if (!uid || !file) { $('#learnMsg').textContent = 'Tap a card and choose a file.'; return; }
  try {
    await api('/api/cards?uid=' + encodeURIComponent(uid) + '&file=' + encodeURIComponent(file) + '&label=' + encodeURIComponent(label), {method:'POST'});
    await api('/api/learn/cancel', {method:'POST'});
    $('#learnBox').hidden = true;
    await refreshCards();
  } catch (e) { $('#learnMsg').textContent = 'Save failed: ' + e.message; }
});

async function refreshSettings() {
  try {
    const s = await api('/api/settings');
    $('#devName').value = s.device;
    $('#setVolume').value = s.volume;
    $('#setVolLabel').textContent = s.volume + ' / 21';
    $('#apPass').value = s.ap_pass;
    $('#webPass').value = '';
    $('#webClear').checked = false;
    $('#webPass').placeholder = s.web_auth ? 'leave empty to keep current' : 'optional';
    $('#wifiNow').textContent = s.ap
      ? ('AP: ' + s.ap_ssid + ' / ' + s.ip)
      : ('Wi-Fi: ' + (s.ssid || '(none)') + ' / ' + s.ip);
  } catch (e) {
    $('#settingsMsg').textContent = e.message;
  }
}

$('#setVolume').addEventListener('input', () => {
  $('#setVolLabel').textContent = $('#setVolume').value + ' / 21';
});

$('#saveSettings').addEventListener('click', async () => {
  const params = new URLSearchParams();
  params.set('device', $('#devName').value);
  params.set('volume', $('#setVolume').value);
  params.set('ap_pass', $('#apPass').value);
  if ($('#webPass').value.length > 0) params.set('web_pass', $('#webPass').value);
  if ($('#webClear').checked) params.set('web_clear', '1');
  try {
    const r = await api('/api/settings?' + params.toString(), {method:'POST'});
    $('#settingsMsg').textContent = r.message || 'Saved.';
    if (r.reboot_required) $('#settingsMsg').textContent += ' Reboot to apply the AP password.';
  } catch (e) { $('#settingsMsg').textContent = 'Save failed: ' + e.message; }
});

$('#scanBtn').addEventListener('click', async () => {
  $('#wifiMsg').textContent = 'Scanning...';
  try {
    const d = await api('/api/wifi/scan');
    const sel = $('#wifiList');
    sel.innerHTML = '<option value="">Scanned networks...</option>';
    (d.networks || []).forEach(n => {
      const o = document.createElement('option');
      o.value = n.ssid;
      o.textContent = n.ssid + ' (' + n.rssi + ' dBm)';
      sel.appendChild(o);
    });
    $('#wifiMsg').textContent = 'Found ' + (d.networks || []).length + ' networks.';
  } catch (e) { $('#wifiMsg').textContent = 'Scan failed: ' + e.message; }
});

$('#wifiList').addEventListener('change', () => {
  if ($('#wifiList').value) $('#wifiSsid').value = $('#wifiList').value;
});

$('#saveWifi').addEventListener('click', async () => {
  const ssid = $('#wifiSsid').value.trim();
  const pass = $('#wifiPass').value;
  if (!ssid) { $('#wifiMsg').textContent = 'Enter a network name.'; return; }
  try {
    await api('/api/wifi?ssid=' + encodeURIComponent(ssid) + '&pass=' + encodeURIComponent(pass), {method:'POST'});
    $('#wifiMsg').textContent = 'Saved. Rebooting; reconnect to the device.';
  } catch (e) { $('#wifiMsg').textContent = 'Save failed: ' + e.message; }
});

$('#rebootBtn').addEventListener('click', async () => {
  if (!confirm('Reboot the player?')) return;
  try { await api('/api/reboot', {method:'POST'}); } catch (e) {}
});

setInterval(refreshStatus, 2000);
refreshStatus();
refreshFiles();
refreshCards();
</script>
</body>
</html>
)RAWHTML";
