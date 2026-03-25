// ── WebSocket setup ──────────────────────────────────────────────
const wsStatusEl      = document.getElementById('ws-status');
const speedBarEl      = document.getElementById('speed-bar');
const speedValEl      = document.getElementById('speed-value');
const rssiBarEls      = [1,2,3,4].map(i => document.getElementById('rssi-bar-' + i));
const rssiValEl       = document.getElementById('rssi-value');
const currentFwdBarEl = document.getElementById('current-fwd-bar');
const currentFwdValEl = document.getElementById('current-fwd-value');
const currentBwdBarEl = document.getElementById('current-bwd-bar');
const currentBwdValEl = document.getElementById('current-bwd-value');

function updateRssi(dbm) {
  rssiValEl.textContent = dbm + ' dBm';
  const filled = dbm > -55 ? 4 : dbm > -65 ? 3 : dbm > -75 ? 2 : 1;
  const cls = filled >= 3 ? 'rssi-strong' : filled === 2 ? 'rssi-fair' : 'rssi-weak';
  rssiBarEls.forEach((bar, i) => {
    bar.className = 'rssi-bar' + (i < filled ? ' ' + cls : '');
  });
}

let ws       = null;
let wsReady  = false;

function connect() {
  ws = new WebSocket(`ws://${location.host}/ws`);

  ws.onopen = () => {
    wsReady = true;
    wsStatusEl.textContent = 'Connected';
    wsStatusEl.className   = 'badge connected';
  };

  ws.onclose = () => {
    wsReady = false;
    wsStatusEl.textContent = 'Disconnected';
    wsStatusEl.className   = 'badge disconnected';
    // Auto-reconnect after 2 s
    setTimeout(connect, 2000);
  };

  ws.onerror = () => ws.close();

  ws.onmessage = (event) => {
    event.data.trim().split(';').forEach(rawMsg => {
      const parts = rawMsg.trim().split(' ');
      if (parts[0] === 'speed') {
        const val = parseInt(parts[1], 10);
        if (!isNaN(val)) {
          const pct = Math.round((val / 255) * 100);
          speedBarEl.style.width = pct + '%';
          speedValEl.textContent = val;
        }
      } else if (parts[0] === 'rssi') {
        const dbm = parseInt(parts[1], 10);
        if (!isNaN(dbm)) updateRssi(dbm);
      } else if (parts[0] === 'current_fwd') {
        const val = parseInt(parts[1], 10);
        if (!isNaN(val)) {
          currentFwdBarEl.style.width = Math.round((val / 255) * 100) + '%';
          currentFwdValEl.textContent = val;
        }
      } else if (parts[0] === 'current_bwd') {
        const val = parseInt(parts[1], 10);
        if (!isNaN(val)) {
          currentBwdBarEl.style.width = Math.round((val / 255) * 100) + '%';
          currentBwdValEl.textContent = val;
        }
      }
    });
  };
}

function send(msg) {
  if (wsReady && ws.readyState === WebSocket.OPEN) {
    ws.send(msg);
  }
}

// ── Steer buttons ────────────────────────────────────────────────
[
  { id: 'btn-left',  press: 'left',   release: 'center' },
  { id: 'btn-right', press: 'right',  release: 'center' },
].forEach(({ id, press, release }) => {
  const btn = document.getElementById(id);
  if (!btn) return;

  let pressed = false;

  const onPress = (e) => {
    e.preventDefault();
    pressed = true;
    btn.classList.add('active');
    send(press);
  };
  const onRelease = (e) => {
    e.preventDefault();
    if (!pressed) return;
    pressed = false;
    btn.classList.remove('active');
    send(release);
  };

  btn.addEventListener('mousedown',   onPress);
  btn.addEventListener('mouseup',     onRelease);
  btn.addEventListener('mouseleave',  onRelease);
  btn.addEventListener('touchstart',  onPress,   { passive: false });
  btn.addEventListener('touchend',    onRelease, { passive: false });
  btn.addEventListener('touchcancel', onRelease, { passive: false });
});

// ── Vertical joystick (drive) ─────────────────────────────────────
const track = document.getElementById('joystick-track');
const knob  = document.getElementById('joystick-knob');

let dragging      = false;
let pointerStartY = 0;
let knobOffset    = 0;   // px from center; negative = up = forward
let lastDriveCmd  = '';  // track last sent drive command to suppress duplicates

function maxOffset() {
  return (track.offsetHeight - knob.offsetHeight) / 2 - 4;
}

function applyOffset(raw) {
  const max  = maxOffset();
  knobOffset = Math.max(-max, Math.min(max, raw));
  knob.style.transform = `translate(-50%, calc(-50% + ${knobOffset}px))`;
  // up (negative offset) → positive drive value (forward)
  const value = -Math.round((knobOffset / max) * 255);
  let cmd;
  if (value > 0)      cmd = `forward ${value}`;
  else if (value < 0) cmd = `backward ${-value}`;
  else                cmd = 'stop';
  if (cmd !== lastDriveCmd) {
    lastDriveCmd = cmd;
    send(cmd);
  }
}

function releaseJoystick() {
  if (!dragging) return;
  dragging   = false;
  knobOffset = 0;
  knob.style.transition = 'transform 0.25s cubic-bezier(0.34, 1.56, 0.64, 1)';
  knob.style.transform  = 'translate(-50%, -50%)';
  if (lastDriveCmd !== 'stop') {
    lastDriveCmd = 'stop';
    send('stop');
  }
}

track.addEventListener('pointerdown', (e) => {
  e.preventDefault();
  dragging = true;
  knob.style.transition = 'none';

  const trackCenterY = track.getBoundingClientRect().top + track.offsetHeight / 2;

  if (e.target === knob) {
    // Grabbed the knob — keep the relative offset under the pointer
    pointerStartY = e.clientY - knobOffset;
  } else {
    // Clicked on the track — jump the knob to that position immediately
    const clickOffset = e.clientY - trackCenterY;
    applyOffset(clickOffset);
    pointerStartY = trackCenterY;
  }

  track.setPointerCapture(e.pointerId);
});

track.addEventListener('pointermove', (e) => {
  if (!dragging) return;
  applyOffset(e.clientY - pointerStartY);
});

track.addEventListener('pointerup',     releaseJoystick);
track.addEventListener('pointercancel', releaseJoystick);

// ── Keyboard shortcuts (desktop) ────────────────────────────────
const keyHeld = new Set();

document.addEventListener('keydown', (e) => {
  if (keyHeld.has(e.key)) return;
  keyHeld.add(e.key);

  if (e.key === 'ArrowLeft')  document.getElementById('btn-left') ?.dispatchEvent(new MouseEvent('mousedown'));
  if (e.key === 'ArrowRight') document.getElementById('btn-right')?.dispatchEvent(new MouseEvent('mousedown'));
  if (e.key === 'ArrowUp')    { knob.style.transition = 'none'; applyOffset(-maxOffset()); }
  if (e.key === 'ArrowDown')  { knob.style.transition = 'none'; applyOffset( maxOffset()); }
});

document.addEventListener('keyup', (e) => {
  keyHeld.delete(e.key);

  if (e.key === 'ArrowLeft')  document.getElementById('btn-left') ?.dispatchEvent(new MouseEvent('mouseup'));
  if (e.key === 'ArrowRight') document.getElementById('btn-right')?.dispatchEvent(new MouseEvent('mouseup'));
  if (e.key === 'ArrowUp' || e.key === 'ArrowDown') releaseJoystick();
});

// ── Keepalive (resets watchdog on ESP32) ───────────────────────
setInterval(() => {
  if (wsReady && ws.readyState === WebSocket.OPEN) ws.send('ping');
}, 1000);

// ── Start ────────────────────────────────────────────────────────
connect();
