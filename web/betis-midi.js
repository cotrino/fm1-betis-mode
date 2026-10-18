/* SPDX-License-Identifier: GPL-3.0-only WITH GPL-3.0-linking-exception */
function adaptMidi(buffer) {
  const data = new Uint8Array(buffer), view = new DataView(data.buffer, data.byteOffset, data.byteLength);
  let at = 0, end = data.length, order = 0;
  const need = n => { if (at + n > end) throw Error('MIDI truncado.'); };
  const byte = () => { need(1); return data[at++]; };
  const u16 = () => { need(2); const n = view.getUint16(at); at += 2; return n; };
  const u32 = () => { need(4); const n = view.getUint32(at); at += 4; return n; };
  const tag = () => String.fromCharCode(byte(), byte(), byte(), byte());
  const vlq = () => {
    let n = 0;
    for (let i = 0; i < 4; i++) { const b = byte(); n = n * 128 + (b & 127); if (!(b & 128)) return n; }
    throw Error('Delta MIDI invalido.');
  };
  if (tag() !== 'MThd') throw Error('No es un archivo MIDI estandar.');
  const header = u32(), format = u16(), tracks = u16(), division = u16();
  if (header < 6 || format > 1 || !tracks || tracks > 256 ||
      (format === 0 && tracks !== 1) || !division || (division & 0x8000))
    throw Error('Solo MIDI tipo 0/1 con tiempo PPQN (no SMPTE), hasta 256 pistas.');
  need(header - 6); at += header - 6;
  const raw = [], names = [];
  let lastTick = 0, ignored = 0;
  for (let track = 0; track < tracks; track++) {
    if (tag() !== 'MTrk') throw Error('Falta una pista MIDI.');
    const size = u32(), trackEnd = at + size;
    if (trackEnd > data.length) throw Error('Pista MIDI truncada.');
    end = trackEnd;
    let tick = 0, running = 0;
    while (at < end) {
      tick += vlq();
      if (tick > 0x0fffffff || raw.length > 200000) throw Error('MIDI demasiado grande.');
      let status = byte();
      if (status < 0x80) { if (!running) throw Error('Running status MIDI invalido.'); at--; status = running; }
      if (status === 0xff) {
        running = 0;
        const type = byte(), length = vlq(); need(length);
        if (type === 0x51) {
          if (length !== 3) throw Error('Tempo MIDI invalido.');
          const tempo = data[at] * 65536 + data[at + 1] * 256 + data[at + 2];
          if (!tempo) throw Error('Tempo MIDI nulo.');
          raw.push({tick, order: order++, tempo});
        }
        if (type === 3) names.push(new TextDecoder().decode(data.slice(at, at + length)));
        at += length;
        if (type === 0x2f) break;
      } else if (status === 0xf0 || status === 0xf7) {
        running = 0; const length = vlq(); need(length); at += length; ignored++;
      } else if (status >= 0x80 && status <= 0xef) {
        running = status;
        const kind = status >> 4, a = byte(), b = kind === 12 || kind === 13 ? 0 : byte();
        if (a > 127 || b > 127) throw Error('Datos MIDI fuera de rango.');
        if (kind === 8 || kind === 9 || (kind === 11 && a === 64))
          raw.push({tick, order: order++, channel: status & 15, kind, a, b});
        else ignored++;
      } else throw Error('Mensaje MIDI no soportado en SMF.');
    }
    lastTick = Math.max(lastTick, tick);
    at = trackEnd; end = data.length;
  }
  let tempo = 500000, tick = 0, ms = 0;
  const events = [], active = new Map(), counts = new Map(), sustained = new Map();
  const pedal = Array(16).fill(false);
  let stolen = 0, noteOns = 0;
  const emit = (time, status, a, b) => events.push({ms: Math.round(time), data: [status, a, b]});
  const release = (key, time) => {
    const note = active.get(key);
    if (!note) return;
    active.delete(key); sustained.delete(key);
    const count = counts.get(note.pitch) - 1;
    if (count) counts.set(note.pitch, count);
    else { counts.delete(note.pitch); emit(time, 0x80, note.pitch, 0); }
  };
  raw.sort((a, b) => a.tick - b.tick || a.order - b.order);
  for (const event of raw) {
    ms += (event.tick - tick) * tempo / division / 1000; tick = event.tick;
    if (ms > 7200000) throw Error('MIDI de mas de dos horas.');
    if (event.tempo) { tempo = event.tempo; continue; }
    const {channel, kind, a, b} = event, key = channel * 128 + a;
    if (kind === 11) {
      pedal[channel] = b >= 64;
      if (!pedal[channel])
        for (const [held, owner] of [...sustained]) if (owner === channel) release(held, ms);
    } else if (channel === 9) {
      emit(ms, kind === 9 && b ? 0x99 : 0x89, a, Math.min(b, 105));
      if (kind === 9 && b) noteOns++;
    } else if (kind === 9 && b) {
      release(key, ms);
      if (!counts.has(a) && counts.size >= 8) {
        const pitch = active.values().next().value.pitch;
        for (const [held, note] of [...active]) if (note.pitch === pitch) { release(held, ms); stolen++; }
      }
      const count = counts.get(a) || 0;
      counts.set(a, count + 1); active.set(key, {pitch: a});
      if (!count) emit(ms, 0x90, a, Math.min(b, 105));
      noteOns++;
    } else if (pedal[channel]) sustained.set(key, channel);
    else release(key, ms);
  }
  const duration = Math.round(ms + (lastTick - tick) * tempo / division / 1000);
  if (!noteOns || duration > 7200000) throw Error('MIDI sin notas o duracion no admitida.');
  for (const key of [...active.keys()]) release(key, duration);
  for (const channel of [0, 9]) emit(duration, 0xb0 | channel, 123, 0);
  return {version: 1, duration, events, report: {
    tracks, names, noteOns, events: events.length, stolen, ignored,
    melodicChannel: 1, drumChannel: 10, maxMelodicVoices: 8,
    arrangement: 'FM monotimbrico de 8 voces + percusion; sin programas, pitch bend ni automatizaciones originales'
  }};
}
function encodeMidi(song) {
  const bytes = [], vlq = value => {
    const out = [value & 127];
    while ((value = Math.floor(value / 128))) out.unshift((value & 127) | 128);
    return out;
  };
  bytes.push(0, 0xff, 0x51, 3, 0x0f, 0x42, 0x40);
  let previous = 0;
  for (const event of song.events) {
    bytes.push(...vlq(event.ms - previous), ...event.data); previous = event.ms;
  }
  bytes.push(0, 0xff, 0x2f, 0);
  const size = bytes.length;
  return new Uint8Array([77,84,104,100,0,0,0,6,0,0,0,1,3,232,
    77,84,114,107,(size >>> 24) & 255,(size >>> 16) & 255,(size >>> 8) & 255,size & 255,...bytes]);
}
globalThis.FM1BetisMidi = {adaptMidi, encodeMidi};
