// ─── Canvas TFT Renderer ──────────────────────────────────────────────────────
// Mirrors the ILI9341 320x240 TFT display in the browser via HTML Canvas.
// All drawing matches the ESP firmware's tft.* calls as closely as possible.

const W = 320, H = 240;
let canvas, ctx;
let currentMode = 1;
let lastData = {};

// Colour palette matching TFT_eSPI defines (RGB→hex)
const C = {
  BLACK:     '#000000',
  WHITE:     '#ffffff',
  CYAN:      '#00ffff',
  YELLOW:    '#ffff00',
  GREEN:     '#00ff00',
  RED:       '#f80000',
  ORANGE:    '#ffa500',
  MAGENTA:   '#fc00fc',
  BLUE:      '#0000ff',
  DARKGREY:  '#404040',
  LIGHTGREY: '#c0c0c0',
  NAVY:      '#000080',
};

// ─── Base Drawing Helpers ────────────────────────────────────────────────────

function clear() {
  ctx.fillStyle = C.BLACK;
  ctx.fillRect(0, 0, W, H);
}

function header(text, color) {
  ctx.fillStyle = color;
  ctx.fillRect(0, 0, W, 20);
  ctx.fillStyle = C.BLACK;
  ctx.font = 'bold 12px monospace';
  ctx.textBaseline = 'middle';
  ctx.textAlign = 'left';
  ctx.fillText(text, 4, 10);
}

function text(x, y, str, color, size) {
  const px = (size || 1) * 8;
  ctx.font = `${size >= 2 ? 'bold ' : ''}${px}px monospace`;
  ctx.fillStyle = color || C.WHITE;
  ctx.textBaseline = 'top';
  ctx.textAlign = 'left';
  ctx.fillText(str, x, y);
}

function textCenter(y, str, color, size) {
  const px = (size || 1) * 8;
  ctx.font = `bold ${px}px monospace`;
  ctx.fillStyle = color || C.WHITE;
  ctx.textBaseline = 'top';
  ctx.textAlign = 'center';
  ctx.fillText(str, W / 2, y);
  ctx.textAlign = 'left';
}

function hline(x, y, w, color) {
  ctx.strokeStyle = color;
  ctx.lineWidth = 1;
  ctx.beginPath(); ctx.moveTo(x, y + 0.5); ctx.lineTo(x + w, y + 0.5); ctx.stroke();
}

function circle(cx, cy, r, color) {
  ctx.strokeStyle = color;
  ctx.lineWidth = 1;
  ctx.beginPath(); ctx.arc(cx, cy, r, 0, Math.PI * 2); ctx.stroke();
}

function dot(cx, cy, r, color) {
  ctx.fillStyle = color;
  ctx.beginPath(); ctx.arc(cx, cy, r, 0, Math.PI * 2); ctx.fill();
}

// ─── Airline Logo Cache ──────────────────────────────────────────────────────

const logoCache = {};   // { ICAO: { img, ok } }

function loadLogo(icao) {
  if (logoCache[icao]) return logoCache[icao];
  const entry = { img: new Image(), ok: false };
  entry.img.crossOrigin = 'anonymous';
  entry.img.onload  = () => { entry.ok = true; };
  entry.img.onerror = () => { entry.ok = false; entry.img = null; };
  entry.img.src = 'https://content.airhex.com/content/logos/airlines_'
                + icao + '_200_200_s.png';
  logoCache[icao] = entry;
  return entry;
}

// ─── Mode Renderers ──────────────────────────────────────────────────────────

function renderFlight(lines) {
  // lines[0]='FLIGHT RADAR', [1]=callsign, [2]=airline?, [3]=route?, then stats
  clear();
  header(' FLIGHT RADAR', C.CYAN);

  let y = 24, i = 1;
  if (!lines[i]) { textCenter(80, 'NO DATA', C.DARKGREY, 1); return; }

  // ── Airline logo (top-right) ──
  const callsign = (lines[i] || '').trim();
  const icao = callsign.replace(/[^A-Z]/gi, '').substring(0, 3).toUpperCase();
  if (icao.length === 3) {
    const logo = loadLogo(icao);
    if (logo.ok && logo.img) {
      const logoSize = 52;
      const lx = W - logoSize - 6, ly = 24;
      // Dark rounded background
      ctx.fillStyle = '#181818';
      ctx.beginPath();
      ctx.roundRect(lx - 4, ly - 4, logoSize + 8, logoSize + 8, 6);
      ctx.fill();
      ctx.drawImage(logo.img, lx, ly, logoSize, logoSize);
    }
  }

  // Callsign big
  text(4, y, callsign || '---', C.WHITE, 2); y += 20; i++;

  // optional airline (yellow) and route (green)
  const nextLines = lines.slice(i);
  let airline = '', route = '';
  for (const l of nextLines) {
    if (/^[A-Z]{2,3}\d+ ?$/.test(l) || l.startsWith('ALT')) break;
    if (!airline) { airline = l; text(4, y, l, C.YELLOW, 1); y += 12; i++; }
    else if (l.includes('>')) { route = l; text(4, y, l, C.GREEN, 1); y += 12; i++; }
  }

  y += 4;
  for (; i < lines.length; i++) {
    const l = lines[i];
    if (!l) continue;
    if (l.startsWith('ALT') || l.startsWith('DIST') || l.startsWith('SPD') ||
        l.startsWith('HDG') || l.startsWith('REG')) {
      text(4, y, l, C.WHITE, 1); y += 13;
    } else {
      // summary entries below separator
      break;
    }
  }

  // Divider
  hline(4, y + 4, 312, C.DARKGREY); y += 10;

  // Secondary aircraft
  for (; i < lines.length; i++) {
    if (!lines[i]) continue;
    text(4, y, lines[i], C.LIGHTGREY, 1); y += 12;
  }
}

function renderAirport(lines) {
  // lines[0]='AIRPORT MONITOR', lines[1]=airport code, lines[2]='name|city|icao|coords', lines[3+]='FLIGHT|ALT ft|DIST nm'
  clear();
  const airport = lines[1] || 'AIRPORT';
  header(' ' + airport + ' ARRIVALS', C.ORANGE);

  let y = 26;
  if (lines.length > 2 && lines[2].includes('|')) {
    const meta = lines[2].split('|');
    if (meta[0]) {
      text(4, y, meta[0], C.LIGHTGREY, 1); y += 12;
      text(4, y, (meta[2] || '') + '  ' + (meta[1] || ''), C.DARKGREY, 1); y += 12;
      text(4, y, (meta[3] || ''), C.DARKGREY, 1); y += 16;
    }
  }

  if (lines.length <= 3 || !lines[3] || lines[3] === 'No arrivals detected') {
    text(4, y, 'No arrivals detected', C.YELLOW, 1);
    return;
  }
  
  // Header line for columns
  ctx.fillStyle = C.DARKGREY; ctx.font = '7px monospace';
  ctx.fillText('FLIGHT', 4, y); ctx.fillText('ALTITUDE', 130, y); ctx.fillText('DISTANCE', 220, y);
  y += 12;

  for (let i = 3; i < lines.length && y < H - 14; i++) {
    const l = lines[i];
    if (!l) continue;
    const parts = l.split('|');
    text(4, y, parts[0] || l, C.WHITE, 1);
    if (parts[1]) text(130, y, parts[1].trim(), C.GREEN, 1);
    if (parts[2]) text(220, y, parts[2].trim(), C.DARKGREY, 1);
    y += 14;
  }
}

function renderRadar(lines, rangeKm) {
  clear();

  // Header
  ctx.fillStyle = C.CYAN;
  ctx.font = '8px monospace';
  ctx.textBaseline = 'top';
  ctx.textAlign = 'left';
  ctx.fillText('RADAR  ' + (rangeKm || '?') + 'km range', 4, 2);

  const cx = 160, cy = 130, maxR = 100;

  // Range rings
  circle(cx, cy, maxR, C.DARKGREY);
  circle(cx, cy, maxR / 2, C.DARKGREY);
  dot(cx, cy, 3, C.WHITE);

  // Crosshairs
  ctx.strokeStyle = C.DARKGREY; ctx.lineWidth = 1;
  ctx.beginPath(); ctx.moveTo(cx - maxR, cy); ctx.lineTo(cx + maxR, cy); ctx.stroke();
  ctx.beginPath(); ctx.moveTo(cx, cy - maxR); ctx.lineTo(cx, cy + maxR); ctx.stroke();

  // Scale labels
  ctx.fillStyle = C.DARKGREY; ctx.font = '7px monospace';
  ctx.textAlign = 'left';
  ctx.fillText((rangeKm || '?') + 'km', cx + maxR + 4, cy - 4);
  ctx.fillText(Math.round((rangeKm || 10) / 2), cx + maxR / 2 - 8, cy + 4);

  const rangeNm = (rangeKm || 10) * 0.54;

  // Aircraft or Airport — each line: "FL123 12.3nm 35000ft 285°" or "[APT] BOM 12.3nm 285°"
  for (let i = 1; i < lines.length; i++) {
    let line = lines[i];
    const isApt = line.startsWith('[APT] ');
    if (isApt) line = line.substring(6).trim();
    
    const parts = line.split(' ');
    if (parts.length < 2) continue;
    const nm = parseFloat(parts[1]);
    const hdg = line.match(/(\d+)°/);
    if (!nm || nm > rangeNm) continue;

    // Approximate pixel position from distance/heading
    const angle = hdg ? (parseFloat(hdg[1]) - 90) * Math.PI / 180 : (i * 60) * Math.PI / 180;
    const r = (nm / rangeNm) * maxR;
    const px = Math.round(cx + r * Math.cos(angle));
    const py = Math.round(cy + r * Math.sin(angle));

    if (isApt) {
      dot(px, py, 4, C.RED);
      ctx.fillStyle = C.RED; ctx.font = '7px monospace'; ctx.textAlign = 'left';
      ctx.fillText(parts[0], px + 5, py - 4);
    } else {
      dot(px, py, 3, C.CYAN);
      ctx.fillStyle = C.WHITE; ctx.font = '7px monospace'; ctx.textAlign = 'left';
      ctx.fillText(parts[0], px + 5, py - 4);
    }
  }
}

function renderWeather(lines) {
  clear();
  header(' WEATHER', C.GREEN);

  let y = 26;
  for (let i = 1; i < lines.length; i++) {
    if (!lines[i]) continue;
    const l = lines[i];
    let color = C.WHITE;
    // prefix matching must align with firmware output: 'Temperature  X', 'Humidity     X', etc.
    if (l.startsWith('AQI')) {
      const v = parseInt(l.match(/\d+/)?.[0] || '0');
      color = v <= 50 ? C.GREEN : v <= 100 ? C.YELLOW : v <= 150 ? C.ORANGE : C.RED;
    } else if (l.startsWith('Temperature')) color = '#ff8c42';
    else if (l.startsWith('Humidity'))     color = '#5bc8f5';
    else if (l.startsWith('UV'))           color = C.YELLOW;
    else if (l.startsWith('Rain'))         color = '#82cfff';

    text(4, y, l, color, 1); y += 17;
  }
}

function renderClock(lines) {
  clear();
  header(' CLOCK', C.MAGENTA);

  // lines[1] = HH:MM:SS, lines[2] = date, lines[3] = timezone
  const timeStr = lines[1] || '--:--:--';
  const dateStr = lines[2] || '';
  const tzStr   = lines[3] || '';

  // Large time — size 4 ~ 32px bold monospace, centered at y=65
  ctx.font = 'bold 32px monospace';
  ctx.fillStyle = C.CYAN;
  ctx.textBaseline = 'top';
  ctx.textAlign = 'center';
  ctx.fillText(timeStr, W / 2, 60);

  ctx.font = 'bold 16px monospace';
  ctx.fillStyle = C.WHITE;
  ctx.fillText(dateStr, W / 2, 108);

  ctx.font = '10px monospace';
  ctx.fillStyle = C.DARKGREY;
  ctx.fillText(tzStr, W / 2, 135);

  ctx.textAlign = 'left';
}

function renderSystem(lines) {
  clear();
  header(' SYSTEM MONITOR', '#888888');

  let y = 26;
  for (let i = 1; i < lines.length; i++) {
    if (!lines[i]) continue;
    const l = lines[i];
    let color = C.WHITE;
    if (l.includes(': OK'))   color = C.GREEN;
    else if (l.includes(': FAIL')) color = C.RED;
    else if (l.startsWith('Signal') || l.startsWith('RSSI')) color = C.CYAN;
    else if (l.startsWith('Ch:'))   color = C.YELLOW;
    else if (l.startsWith('SSID:') || l.startsWith('Heap:') || l.startsWith('Last')) color = C.LIGHTGREY;
    text(4, y, l, color, 1); y += 13;
  }
}

function renderLoading() {
  clear();
  ctx.fillStyle = C.DARKGREY;
  ctx.font = '10px monospace';
  ctx.textBaseline = 'middle';
  ctx.textAlign = 'center';
  ctx.fillText('Waiting for data...', W / 2, H / 2);
  ctx.textAlign = 'left';
}

// ─── Preview Toggle ─────────────────────────────────────────────────────
let previewMode = 'canvas'; // 'canvas' | 'text'
function togglePreview() {
  const btn    = document.getElementById('preview-toggle');
  const cvWrap = document.getElementById('canvas-wrap');
  const txWrap = document.getElementById('text-wrap');
  if (previewMode === 'canvas') {
    previewMode = 'text';
    cvWrap.style.display = 'none';
    txWrap.style.display = 'block';
    btn.textContent = '📄 Text View';
    // redraw canvas immediately when switching back
    if (lastData && lastData.preview) renderPreview(lastData);
  }
}

function updateLiveView(preview) {
  const lv = document.getElementById('live-text');
  if (lv) lv.textContent = preview || '(no data)';
}

// ─── Render Dispatch ─────────────────────────────────────────────────────────

function renderPreview(j) {
  if (!ctx) return;
  const raw = (j.preview || '').trim();
  if (!raw) { renderLoading(); return; }

  const lines = raw.split('\n').map(l => l.trim());

  switch (j.mode) {
    case 1: renderFlight(lines); break;
    case 2: renderAirport(lines); break;
    case 3: renderRadar(lines, j.range_km); break;
    case 4: renderWeather(lines); break;
    case 5: renderClock(lines); break;
    case 6: renderSystem(lines); break;
    default: renderLoading();
  }
}

// ─── Toast ────────────────────────────────────────────────────────────────────
function toast(msg) {
  const t = document.getElementById('toast');
  t.textContent = msg;
  t.classList.add('show');
  setTimeout(() => t.classList.remove('show'), 2200);
}

// ─── Poll Status ──────────────────────────────────────────────────────────────
async function updateStatus() {
  try {
    const r = await fetch('/api/status');
    const j = await r.json();
    lastData = j;

    // Show an indicator while the ESP is fetching new data
    const updInd = document.getElementById('upd-ind');
    if (updInd) updInd.style.display = j.updating ? 'inline' : 'none';

    if (!j.updating) {
      // Only redraw when the update is complete — avoids stale/torn preview
      renderPreview(j);
      updateLiveView(j.preview);
    }

    document.getElementById('ip-badge').textContent = j.ip;
    document.getElementById('rssi-badge').textContent = j.rssi + ' dBm';
    document.getElementById('heap').textContent = Math.round(j.heap / 1024) + ' KB';
    document.getElementById('ssid').textContent = j.ssid || '—';
    document.getElementById('ch').textContent = j.channel;
    document.getElementById('adsb-st').textContent = j.adsb_ok ? '✓ OK' : '✗ FAIL';
    document.getElementById('adsb-st').style.color = j.adsb_ok ? '#0f0' : '#f44';
    document.getElementById('wx-st').textContent = j.weather_ok ? '✓ OK' : '✗ FAIL';
    document.getElementById('wx-st').style.color = j.weather_ok ? '#0f0' : '#f44';

    const mins = Math.floor(j.uptime / 60);
    const hrs = Math.floor(mins / 60);
    document.getElementById('uptime').textContent =
      hrs > 0 ? hrs + 'h ' + (mins % 60) + 'm' : mins + 'm';

    document.getElementById('conn').textContent = '●';
    document.getElementById('conn').className = 'badge badge-ok';

    if (j.mode !== currentMode) {
      currentMode = j.mode;
      highlightMode(currentMode);
    }
  } catch (e) {
    document.getElementById('conn').textContent = '●';
    document.getElementById('conn').className = 'badge badge-err';
  }
}

function highlightMode(m) {
  document.querySelectorAll('.mode-btn').forEach(b => {
    b.classList.toggle('active', parseInt(b.dataset.m) === m);
  });
}

// ─── Mode Switch ──────────────────────────────────────────────────────────────
document.querySelectorAll('.mode-btn').forEach(btn => {
  btn.addEventListener('click', async () => {
    const m = btn.dataset.m;
    await fetch('/api/mode?m=' + m);
    currentMode = parseInt(m);
    highlightMode(currentMode);
    toast('Mode ' + m + ' set');
    setTimeout(updateStatus, 1500);
  });
});

// ─── Load Config ──────────────────────────────────────────────────────────────
async function loadConfig() {
  try {
    const r = await fetch('/api/config');
    const c = await r.json();
    document.getElementById('lat').value = c.lat;
    document.getElementById('lon').value = c.lon;
    document.getElementById('range').value = c.range;
    document.getElementById('timezone').value = c.timezone || 'Asia/Kolkata';
    document.getElementById('timezone').dataset.offset = c.tzOffset || 19800;
    document.getElementById('wifi-ssid').value = c.wifi_ssid || '';
    
    if (document.getElementById('units')) document.getElementById('units').value = c.units || 0;
    if (document.getElementById('f-ground')) document.getElementById('f-ground').checked = c.f_ground || false;
    if (document.getElementById('f-glider')) document.getElementById('f-glider').checked = c.f_glider || false;
    if (document.getElementById('btn-pin')) document.getElementById('btn-pin').value = c.btn_pin !== undefined ? c.btn_pin : 0;

    currentMode = c.mode;
    highlightMode(currentMode);
  } catch (e) {
    console.error('Config load failed', e);
  }
}

// ─── Save Config ──────────────────────────────────────────────────────────────
async function saveConfig() {
  const params = new URLSearchParams({
    lat:      document.getElementById('lat').value,
    lon:      document.getElementById('lon').value,
    range:    document.getElementById('range').value,
    timezone: document.getElementById('timezone').value,
    tzOffset: document.getElementById('timezone').dataset.offset || 19800,
    units:    document.getElementById('units').value,
    f_ground: document.getElementById('f-ground').checked,
    f_glider: document.getElementById('f-glider').checked,
    btn_pin:  document.getElementById('btn-pin').value
  });
  try {
    const r = await fetch('/api/config/save', {
      method: 'POST',
      headers: { 'Content-Type': 'application/x-www-form-urlencoded' },
      body: params.toString()
    });
    const saved = await r.json();
    // Reload form fields from what the ESP actually stored
    document.getElementById('lat').value      = saved.lat;
    document.getElementById('lon').value      = saved.lon;
    document.getElementById('range').value    = saved.range;
    document.getElementById('timezone').value = saved.timezone || '';
    if (saved.tzOffset) document.getElementById('timezone').dataset.offset = saved.tzOffset;

    if (saved.units !== undefined)  document.getElementById('units').value = saved.units;
    if (saved.f_ground !== undefined) document.getElementById('f-ground').checked = saved.f_ground;
    if (saved.f_glider !== undefined) document.getElementById('f-glider').checked = saved.f_glider;
    if (saved.btn_pin !== undefined) document.getElementById('btn-pin').value = saved.btn_pin;

    toast('Settings saved \u2713');
  } catch (e) {
    toast('Save failed \u2014 check connection');
    return;
  }
  // Fetch fresh status so preview reflects new settings immediately
  setTimeout(updateStatus, 1800);
}

// ─── Auto-Locate ──────────────────────────────────────────────────────────────
async function autoLocate() {
  toast('Locating...');
  try {
    const res = await fetch('http://ip-api.com/json/?fields=lat,lon,timezone,offset');
    if (!res.ok) throw new Error('ip-api error');
    const data = await res.json();
    document.getElementById('lat').value = data.lat || 19.076;
    document.getElementById('lon').value = data.lon || 72.877;
    document.getElementById('timezone').value = data.timezone || 'Asia/Kolkata';
    document.getElementById('timezone').dataset.offset = data.offset || 19800;
    toast('Location found. Don\'t forget to save!');
  } catch (err) {
    toast('Location failed \u2014 check connection/adblock');
    console.error('Auto-locate error:', err);
  }
}

// ─── Save Credentials ─────────────────────────────────────────────────────────
async function saveCredentials() {
  const user = document.getElementById('dash-user').value;
  const pass = document.getElementById('dash-pass').value;
  if (!user || !pass) { toast('Enter Username & Password'); return; }
  const params = new URLSearchParams({ user, pass });
  try {
    const r = await fetch('/api/credentials?' + params);
    if (!r.ok) throw new Error('save failed');
    toast('Login updated! Reloading...');
    setTimeout(() => window.location.reload(), 1500);
  } catch (e) {
    toast('Update failed');
  }
}

// ─── Save WiFi ────────────────────────────────────────────────────────────────
async function saveWifi() {
  const ssid = document.getElementById('wifi-ssid').value;
  const pass = document.getElementById('wifi-pass').value;
  if (!ssid) { toast('Enter SSID'); return; }
  const params = new URLSearchParams({ ssid, pass });
  await fetch('/api/wifi?' + params);
  toast('WiFi saved — rebooting...');
}

// ─── Init ─────────────────────────────────────────────────────────────────────
window.addEventListener('DOMContentLoaded', () => {
  canvas = document.getElementById('tft');
  ctx = canvas.getContext('2d');
  renderLoading();
  loadConfig();
  updateStatus();
  setInterval(updateStatus, 3000);
});