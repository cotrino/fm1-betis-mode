"""Compile a user-provided SMF melody into a private FM1 lesson/song table."""
import argparse
import hashlib
import json
import struct
from pathlib import Path


def read_midi(path):
    data = path.read_bytes()
    if len(data) < 14 or data[:4] != b"MThd":
        raise ValueError("Expected a standard MIDI file")
    length, kind, tracks, division = struct.unpack(">IHHH", data[4:14])
    if length < 6 or kind not in (0, 1) or not tracks or not division or division & 0x8000:
        raise ValueError("Only type 0/1 PPQN MIDI is supported")
    at = 8 + length
    notes, tempos = [], [(0, 500000)]
    for track in range(tracks):
        if data[at:at + 4] != b"MTrk":
            raise ValueError("Missing MIDI track")
        size = struct.unpack(">I", data[at + 4:at + 8])[0]
        at += 8
        end = at + size
        if end > len(data):
            raise ValueError("Truncated MIDI track")
        tick, running = 0, 0

        def byte():
            nonlocal at
            if at >= end:
                raise ValueError("Truncated MIDI event")
            value = data[at]
            at += 1
            return value

        def vlq():
            value = 0
            for _ in range(4):
                b = byte()
                value = value * 128 + (b & 127)
                if b < 128:
                    return value
            raise ValueError("Invalid MIDI VLQ")

        while at < end:
            tick += vlq()
            status = byte()
            if status < 128:
                if not running:
                    raise ValueError("Invalid running status")
                at -= 1
                status = running
            if status == 255:
                running = 0
                event, count = byte(), vlq()
                if at + count > end:
                    raise ValueError("Truncated meta event")
                if event == 81:
                    if count != 3:
                        raise ValueError("Invalid tempo")
                    tempo = int.from_bytes(data[at:at + count], "big")
                    if not tempo:
                        raise ValueError("Zero MIDI tempo")
                    tempos.append((tick, tempo))
                at += count
                if event == 47:
                    break
            elif status in (240, 247):
                running = 0
                count = vlq()
                if at + count > end:
                    raise ValueError("Truncated SysEx")
                at += count
            elif 128 <= status < 240:
                running = status
                a = byte()
                b = 0 if status >> 4 in (12, 13) else byte()
                if a > 127 or b > 127:
                    raise ValueError("Invalid MIDI data")
                if status & 240 == 144 and b and status & 15 != 9:
                    notes.append((track, tick, a))
            else:
                raise ValueError("Unsupported SMF message")
        at = end
    return notes, sorted(tempos, key=lambda t: t[0]), division


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("midi", type=Path)
    parser.add_argument("--track", type=int, default=1, help="1-based melody track; inspect original score")
    parser.add_argument("--output", type=Path, required=True)
    args = parser.parse_args()
    notes, tempos, division = read_midi(args.midi)
    chosen = [(tick, note) for track, tick, note in notes if track == args.track - 1]
    if not chosen:
        raise ValueError("Selected melody track contains no pitched notes")
    # A chord at one onset becomes its highest pitch, not the accompaniment.
    onsets = {}
    for tick, note in chosen:
        onsets[tick] = max(note, onsets.get(tick, 0))
    if len(onsets) > 4096:
        raise ValueError("Melody exceeds 4096 steps")
    events, previous_tick, elapsed, tempo_index, tempo = [], 0, 0.0, 0, 500000
    for tick, note in sorted(onsets.items()):
        while tempo_index < len(tempos) and tempos[tempo_index][0] <= tick:
            change, next_tempo = tempos[tempo_index]
            elapsed += (change - previous_tick) * tempo / division / 1000
            previous_tick, tempo = change, next_tempo
            tempo_index += 1
        elapsed += (tick - previous_tick) * tempo / division / 1000
        previous_tick = tick
        events.append((round(elapsed), note))
    if events[-1][0] > 3600000:
        raise ValueError("Melody exceeds one hour")
    # Fold octaves into the actual 27-key FM1 range while preserving pitch class.
    folded = [(ms, note + 12 * ((53 - note + 11) // 12) if note < 53 else
               note - 12 * ((note - 79 + 11) // 12) if note > 79 else note)
              for ms, note in events]
    start = folded[0][0]
    rows = []
    for i, (ms, note) in enumerate(folded):
        duration = folded[i + 1][0] - ms if i + 1 < len(folded) else 600
        if not 1 <= duration <= 65535:
            raise ValueError("Melody step duration must fit 16 bits")
        rows.append(f"    {{{duration}, {note}}},")
    text = (
        "/* Private user-supplied melody; generated by tools/Compile-Hymn.py. */\n"
        f"#define BETIS_SONG_COUNT {len(rows)}U\n"
        "static const struct betis_song_step betis_song[] = {\n"
        + "\n".join(rows) + "\n};\n"
    )
    args.output.parent.mkdir(parents=True, exist_ok=True)
    args.output.write_text(text, encoding="ascii", newline="\n")
    print(json.dumps({"steps": len(rows), "melody_track": args.track,
                      "original_track_counts": {str(t + 1): sum(n[0] == t for n in notes)
                                                for t in sorted({n[0] for n in notes})},
                      "duration_ms": folded[-1][0] - start + 600,
                      "octave_folded": sum(a[1] != b[1] for a, b in zip(events, folded)),
                      "input_sha256": hashlib.sha256(args.midi.read_bytes()).hexdigest()}))


if __name__ == "__main__":
    main()
