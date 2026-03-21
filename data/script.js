// ─── Canvas TFT Renderer ──────────────────────────────────────────────────────
// Mirrors the ILI9341 320x240 TFT display in the browser via HTML Canvas.
// All drawing matches the ESP firmware's tft.* calls as closely as possible.
const W = 320, H = 240;
let canvas, ctx;
let currentMode = 1;
let lastData = {};
// Colour palette matching TFT_eSPI defines (RGB→hex)
const C = {
  BLACK: '#000000',
  WHITE: '#ffffff',
  CYAN: '#00ffff',
  YELLOW: '#ffff00',
  GREEN: '#00ff00',
  RED: '#f80000',
  ORANGE: '#ffa500',
  MAGENTA: '#fc00fc',
  BLUE: '#0000ff',
  DARKGREY: '#404040',
  LIGHTGREY: '#c0c0c0',
  NAVY: '#000080',
};
// ─── Base Drawing Helpers ────────────────────────────────────────────────────
function clear() {
  ctx.fillStyle = C.BLACK;
  ctx.fillRect(0, 0, W, H);
}
function header(text, color) {
  ctx.fillStyle = color;
  ctx.fillRect(0, 0, W, 22);
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
  entry.img.onload = () => { entry.ok = true; };
  entry.img.onerror = () => { entry.ok = false; entry.img = null; };
  entry.img.src = 'https://images.flightradar24.com/assets/airlines/logotypes/'
    + icao + '_logo0.png';
  logoCache[icao] = entry;
  return entry;
}
// ─── Mode Renderers ──────────────────────────────────────────────────────────
function renderFlight(rawLines) {
  clear();
  if (rawLines.length < 2) {
    header(' OVERHEAD TRACKER', C.YELLOW);
    textCenter(80, 'NO DATA', C.DARKGREY, 1);
    return;
  }
  const lines = rawLines[1].split('|');
  if (lines.length < 2) {
    header(' OVERHEAD TRACKER', C.YELLOW);
    textCenter(80, 'NO DATA', C.DARKGREY, 1);
    return;
  }
  const callsign = lines[0] || '---';
  const route = (lines[1] || '').replace('-', ' > ');
  const index = lines[2] || '1/1';
  const alt = lines[3] || '';
  const vspd = lines[4] || '';
  const spd = lines[5] || '';
  const dist = lines[6] || '';
  const reg = lines[7] || '';
  const type = lines[8] || '';
  const coords = lines[9] || '';
  const hdg = lines[10] || '';
  const airline = lines[11] || '';
  const oFull = lines[12] || '';
  const oCountry = lines[13] || '';
  const dFull = lines[14] || '';
  const dCountry = lines[15] || '';
  ctx.fillStyle = C.YELLOW;
  ctx.fillRect(0, 0, W, 22);
  ctx.fillStyle = C.BLACK;
  ctx.font = 'bold 12px monospace';
  ctx.textBaseline = 'middle';
  ctx.textAlign = 'left';
  ctx.fillText('OVERHEAD TRACKER', 4, 11);
  ctx.font = '10px monospace';
  ctx.textAlign = 'right';
  ctx.fillText(index, 316, 11);
  ctx.textAlign = 'left';
  let y = 26;
  // Row 1: Callsign & Airline
  text(4, y, callsign, C.YELLOW, 3);
  y += 24;
  text(4, y, airline ? airline : 'UNKNOWN', C.ORANGE, 2);
  y = 86;
  hline(4, y, 250, C.CYAN); y += 4;
  // Row 2: Type, Reg, Route
  text(4, y, 'AIRCRAFT TYPE', C.LIGHTGREY, 1);
  text(110, y, 'REGISTRATION', C.LIGHTGREY, 1);
  text(200, y, 'ROUTE', C.LIGHTGREY, 1);
  y += 10;
  text(4, y, type ? type.substring(0, 8) : '???', C.CYAN, 2);
  text(110, y, reg ? reg : '???', C.YELLOW, 2);
  text(200, y, route ? route.substring(0,11) : 'UNKNOWN', '#adff2f', 2);
  y += 18;
  hline(4, y, 312, C.CYAN); y += 4;
  // Row 3: Airports
  const drawApt = (curY, fullName, country) => {
    if (!fullName) return;
    if (country) {
      const flagImg = new Image();
      flagImg.src = `https://flagcdn.com/w40/${country.toLowerCase()}.png`;
      flagImg.onload = () => ctx.drawImage(flagImg, 4, curY - 2, 20, 14);
    }
    text(28, curY, fullName.substring(0, 48), C.CYAN, 1);
  };
  if (oFull || dFull) {
    drawApt(y, oFull, oCountry); y += 14;
    drawApt(y, dFull, dCountry); y += 14;
  } else {
    y += 28;
  }
  hline(4, y, 312, C.CYAN); y += 4;
  // Row 4: Phase, Altitude, Speed, Distance
  const altNum = parseFloat(alt) || 0;
  const vsVal = parseInt(vspd) || 0;
  let phase = "CRUISE";
  let phaseColor = C.GREEN;
  if (altNum < 4000 && vsVal < -250) { phase = "LANDING"; phaseColor = C.RED; }
  else if (altNum < 4000 && vsVal > 250) { phase = "TAKEOFF"; phaseColor = C.ORANGE; }
  else if (vsVal > 250) { phase = "CLIMBING"; phaseColor = C.ORANGE; }
  else if (vsVal < -250) { phase = "DESCENT"; phaseColor = C.RED; }
  text(4, y, 'PHASE', C.LIGHTGREY, 1);
  text(110, y, 'ALTITUDE', C.LIGHTGREY, 1);
  text(200, y, 'SPEED', C.LIGHTGREY, 1);
  text(260, y, 'DIST', C.LIGHTGREY, 1);
  y += 10;
  text(4, y, phase, phaseColor, 2);
  text(110, y, alt, C.YELLOW, 2);
  text(200, y, spd, C.YELLOW, 2);
  text(260, y, dist, C.CYAN, 2);
  y += 18;
  const vsColor = vsVal > 100 ? C.GREEN : (vsVal < -100 ? C.RED : C.WHITE);
  text(4, y, vspd, vsColor, 1);
  if (hdg !== '---') {
    text(110, y, 'HDG: ' + hdg + '°', C.LIGHTGREY, 1);
  }
  y += 12;
  hline(4, y, 312, C.DARKGREY); y += 6;
  // Secondary Aircraft
  const count = rawLines.length - 2;
  if (count > 1) {
    for (let j = 2; j < rawLines.length; j++) {
      const rline = rawLines[j];
      if (rline.startsWith("SEC|")) {
        const p = rline.split('|');
        text(4, y, p[1], C.WHITE, 2);
        text(85, y, p[2], '#adff2f', 2);
        text(155, y, p[3], C.YELLOW, 2);
        text(205, y, p[4], C.LIGHTGREY, 2);
        text(255, y, p[5], C.CYAN, 2);
        y += 18;
      } else {
        text(4, y, rline, C.WHITE, 1);
        y += 12;
      }
      if (y > 230) break;
    }
  }
  // Draw Logo EARLY
  const icao = callsign.replace(/[^A-Z]/gi, '').substring(0, 3).toUpperCase();
  if (icao.length === 3) {
    const logo = loadLogo(icao);
    if (logo.ok && logo.img) {
      let dw = 50, dh = 50;
      const nw = logo.img.naturalWidth || logo.img.width || 1;
      const nh = logo.img.naturalHeight || logo.img.height || 1;
      const aspect = nw / nh;
      if (aspect > 1) dh = 50 / aspect;
      else dw = 50 * aspect;
      const dx = 260 + (50 - dw) / 2;
      const dy = 30 + (50 - dh) / 2;
      ctx.drawImage(logo.img, dx, dy, dw, dh);
    }
  }
}
function renderAirport(lines) {
  // lines[0]='AIRPORT MONITOR', lines[1]=airport code, lines[2]='name|city|icao|coords', lines[3+]='FLIGHT|ALT ft|DIST nm'
  clear();
  const airport = lines[1] || 'AIRPORT';
  header(' ' + airport + ' ARRIVALS', C.ORANGE);
  let y = 28;
  if (lines.length > 2 && lines[2].includes('|')) {
    const meta = lines[2].split('|');
    if (meta[0]) {
      text(4, y, (meta[1] || ''), C.WHITE, 2); y += 20;
      text(4, y, meta[0], C.LIGHTGREY, 1); y += 12;
      text(4, y, `ICAO:${meta[2] || ''}  IATA:${airport}  ALT:${meta[4] || ''}ft`, C.CYAN, 1); y += 16;
    }
  }
  if (lines.length <= 3 || !lines[3] || lines[3] === 'No arrivals detected') {
    text(4, y, 'No arrivals detected', C.YELLOW, 1);
    return;
  }
  let hasArr = false, hasDep = false;
  for (let i = 3; i < lines.length && y < H - 16; i++) {
    let l = lines[i];
    if (!l) continue;
    if (l.startsWith('ARR:') && !hasArr) {
      text(4, y, 'ARRIVALS', C.CYAN, 1); y += 12; hasArr = true;
      l = l.substring(4);
    } else if (l.startsWith('DEP:') && !hasDep) {
      text(4, y, 'DEPARTURES', C.MAGENTA, 1); y += 12; hasDep = true;
      l = l.substring(4);
    } else if (l.startsWith('ARR:') || l.startsWith('DEP:')) {
      l = l.substring(4);
    }
    const parts = l.split('|');
    text(4, y, (parts[0] || l).substring(0,8), C.WHITE, 2);
    if (parts[1]) text(120, y, parts[1].trim(), C.GREEN, 2);
    if (parts[2]) text(220, y, parts[2].trim(), C.DARKGREY, 2);
    y += 18;
  }
}
function renderRadar(lines, rangeKm) {
  clear();
  // Header
  ctx.fillStyle = C.CYAN;
  ctx.font = 'bold 16px monospace';
  ctx.textBaseline = 'top';
  ctx.textAlign = 'left';
  ctx.fillText('RADAR  ' + (rangeKm || '?') + 'km', 4, 1);
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
  // Aircraft or Airport
  for (let i = 1; i < lines.length; i++) {
    let line = lines[i];
    const isApt = line.startsWith('[APT] ');
    if (isApt) line = line.substring(6).trim();
    console.log(`Radar Line: ${line} (Apt: ${isApt})`);
    const nmMatch = line.match(/(\d+\.?\d*)nm/);
    if (!nmMatch) continue;
    const nm = parseFloat(nmMatch[1]);
    if (nm > rangeNm) continue;
    // Position: B{bearing}
    // Heading:  T{track}
    const bMatch = line.match(/ B(\d+)/);
    const tMatch = line.match(/ T(\d+)/);
    // Bearing for position (0 is North)
    const bearing = bMatch ? parseFloat(bMatch[1]) : (i * 60);
    const bRad = (bearing - 90) * Math.PI / 180;
    const r = (nm / rangeNm) * maxR;
    const px = Math.round(cx + r * Math.cos(bRad));
    const py = Math.round(cy + r * Math.sin(bRad));
    if (isApt) {
      dot(px, py, 4, C.RED);
      ctx.fillStyle = C.RED; ctx.font = 'bold 16px monospace'; ctx.textAlign = 'left';
      ctx.fillText(line.split(' ')[0] || 'APT', px + 6, py - 6);
    } else {
      ctx.fillStyle = C.CYAN;
      if (tMatch) {
        const track = parseFloat(tMatch[1]);
        const tRad = (track - 90) * Math.PI / 180;
        ctx.strokeStyle = C.CYAN;
        ctx.beginPath();
        ctx.moveTo(px, py);
        ctx.lineTo(px + Math.cos(tRad) * 10, py + Math.sin(tRad) * 10);
        ctx.stroke();
        dot(px, py, 2, C.CYAN);
      } else {
        dot(px, py, 3, C.CYAN);
      }
      ctx.fillStyle = C.YELLOW; ctx.font = 'bold 16px monospace'; ctx.textAlign = 'left';
      ctx.fillText(line.split(' ')[0], px + 6, py - 6);
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
    else if (l.startsWith('Humidity')) color = '#5bc8f5';
    else if (l.startsWith('UV')) color = C.YELLOW;
    else if (l.startsWith('Precipitation')) color = '#82cfff';
    else if (l.startsWith('Wind')) color = '#82cfff';
    else if (l.startsWith('Visibility')) color = '#ffffff';
    text(4, y, l, color, 2); y += 22;
  }
}
function renderClock(lines) {
  clear();
  header(' CLOCK', C.CYAN);
  // lines[1] = HH:MM:SS, lines[2] = date, lines[3] = fullTz, lines[4] = timezone
  const timeStr = lines[1] || '--:--:--';
  const dateStr = lines[2] || '';
  const fullTz  = lines[3] || '';
  const tzStr   = lines[4] || '';

  ctx.font = 'bold 48px monospace';
  ctx.fillStyle = C.CYAN;
  ctx.textBaseline = 'top';
  ctx.textAlign = 'center';
  ctx.fillText(timeStr, W / 2, 50);

  ctx.font = 'bold 20px monospace';
  ctx.fillStyle = C.YELLOW;
  ctx.fillText(dateStr, W / 2, 115);

  ctx.font = '16px monospace';
  ctx.fillStyle = C.WHITE;
  ctx.fillText(fullTz, W / 2, 165);

  ctx.font = '14px monospace';
  ctx.fillStyle = C.LIGHTGREY;
  ctx.fillText(tzStr, W / 2, 195);
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
    if (l.includes(': OK')) color = C.GREEN;
    else if (l.includes(': FAIL')) color = C.RED;
    else if (l.startsWith('Signal') || l.startsWith('RSSI')) color = C.CYAN;
    else if (l.startsWith('Ch:')) color = C.YELLOW;
    else if (l.startsWith('SSID:') || l.startsWith('Heap:') || l.startsWith('Last')) color = C.LIGHTGREY;
    text(4, y, l, color, 2); y += 18;
  }
}
function renderLoading() {
  clear();
  ctx.fillStyle = C.LIGHTGREY;
  ctx.font = '10px monospace';
  ctx.textBaseline = 'middle';
  ctx.textAlign = 'center';
  ctx.fillText('Waiting for data...', W / 2, H / 2);
  ctx.textAlign = 'left';
}
// ─── Preview Toggle ─────────────────────────────────────────────────────
let previewMode = 'canvas'; // 'canvas' | 'text'
function togglePreview() {
  const btn = document.getElementById('preview-toggle');
  const cvWrap = document.getElementById('canvas-wrap');
  const txWrap = document.getElementById('text-wrap');
  if (previewMode === 'canvas') {
    previewMode = 'text';
    cvWrap.style.display = 'none';
    txWrap.style.display = 'block';
    btn.textContent = '🎨 Canvas View';
    // redraw text view immediately
    if (lastData && lastData.preview) updateLiveView(lastData.preview);
  } else {
    previewMode = 'canvas';
    cvWrap.style.display = 'block';
    txWrap.style.display = 'none';
    btn.textContent = '📄 Text View';
    // redraw canvas immediately when switching back
    if (lastData && lastData.preview) renderPreview(lastData);
  }
}
function updateLiveView(preview) {
  const pt = document.getElementById('preview-text');
  if (pt) pt.textContent = preview || '(no data)';
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
    if (document.getElementById('sys-ip')) document.getElementById('sys-ip').textContent = j.ip;
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
  const tzVal = document.getElementById('timezone').value.trim();
  let tzOff = parseInt(document.getElementById('timezone').dataset.offset || '19800', 10);
  let tzAbbr = document.getElementById('timezone').dataset.abbr || 'IST';
  try {
    const d = new Date();
    const tzFormat = new Intl.DateTimeFormat('en-US', { timeZone: tzVal, hour12: false, year: 'numeric', month: 'numeric', day: 'numeric', hour: 'numeric', minute: 'numeric', second: 'numeric' });
    const utcFormat = new Intl.DateTimeFormat('en-US', { timeZone: 'UTC', hour12: false, year: 'numeric', month: 'numeric', day: 'numeric', hour: 'numeric', minute: 'numeric', second: 'numeric' });
    
    const parseDate = (fmt) => {
        const parts = fmt.formatToParts(d);
        const p = {};
        parts.forEach(part => p[part.type] = part.value);
        return Date.UTC(p.year, p.month - 1, p.day, p.hour, p.minute, p.second);
    };
    
    tzOff = Math.round((parseDate(tzFormat) - parseDate(utcFormat)) / 1000);
    
    const formatter = new Intl.DateTimeFormat('en-US', { timeZoneName: 'short', timeZone: tzVal });
    const parts = formatter.formatToParts();
    const part = parts.find(p => p.type === 'timeZoneName');
    if (part) tzAbbr = part.value;

    document.getElementById('timezone').dataset.offset = tzOff;
    document.getElementById('timezone').dataset.abbr = tzAbbr;
  } catch(e) { 
    console.log("Invalid timezone string, using fallback. " + e);
  }

  const params = new URLSearchParams({
    lat: document.getElementById('lat').value,
    lon: document.getElementById('lon').value,
    range: document.getElementById('range').value,
    timezone: tzVal,
    tzOffset: tzOff,
    tzAbbr: tzAbbr,
    units: document.getElementById('units').value,
    f_ground: document.getElementById('f-ground').checked,
    f_glider: document.getElementById('f-glider').checked,
    btn_pin: document.getElementById('btn-pin').value
  });
  try {
    const r = await fetch('/api/config/save', {
      method: 'POST',
      headers: { 'Content-Type': 'application/x-www-form-urlencoded' },
      body: params.toString()
    });
    const saved = await r.json();
    // Reload form fields from what the ESP actually stored
    document.getElementById('lat').value = saved.lat;
    document.getElementById('lon').value = saved.lon;
    document.getElementById('range').value = saved.range;
    document.getElementById('timezone').value = saved.timezone || '';
    if (saved.tzOffset) document.getElementById('timezone').dataset.offset = saved.tzOffset;
    if (saved.tzAbbr) document.getElementById('timezone').dataset.abbr = saved.tzAbbr;
    if (saved.units !== undefined) document.getElementById('units').value = saved.units;
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
// ─── Reboot Device ────────────────────────────────────────────────────────────
async function rebootDevice() {
  if (!confirm("Are you sure you want to reboot the ESP32?")) return;
  toast("Rebooting device...");
  try {
    await fetch("/api/reboot", { method: "POST" });
  } catch(e) {}
  setTimeout(() => location.reload(), 4000);
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
