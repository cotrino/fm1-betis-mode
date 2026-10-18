"""Compile isolated MIDI melodies into compact FM1 teaching tables."""
import argparse
from collections import defaultdict, deque
from dataclasses import dataclass
import hashlib
import json
import struct
from pathlib import Path


@dataclass
class Note:
    track: int
    channel: int
    tick: int
    pitch: int
    end: int = 0


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
        active, sustained = defaultdict(deque), defaultdict(list)
        pedal = [False] * 16

        def release(channel):
            for note in sustained[channel]:
                note.end = tick
            sustained[channel].clear()

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
                channel, kind = status & 15, status >> 4
                if channel == 9:
                    continue
                if kind == 9 and b:
                    note = Note(track, channel, tick, a)
                    active[channel, a].append(note)
                    notes.append(note)
                elif kind == 8 or kind == 9 and not b:
                    if active[channel, a]:
                        note = active[channel, a].popleft()
                        if pedal[channel]:
                            sustained[channel].append(note)
                        else:
                            note.end = tick
                elif kind == 11 and a == 64:
                    pedal[channel] = b >= 64
                    if not pedal[channel]:
                        release(channel)
            else:
                raise ValueError("Unsupported SMF message")
        for queue in active.values():
            for note in queue:
                note.end = tick
        for channel in range(16):
            release(channel)
        at = end
    return notes, sorted(tempos, key=lambda t: t[0]), division


def milliseconds(tick, tempos, division):
    elapsed, previous, tempo = 0, 0, 500000
    for change, next_tempo in tempos:
        if change > tick:
            break
        elapsed += (change - previous) * tempo
        previous, tempo = change, next_tempo
    return round((elapsed + (tick - previous) * tempo) / division / 1000)


def melody(path, track=1, minimum=0):
    notes, tempos, division = read_midi(path)
    onsets = {}
    for note in notes:
        if note.track != track - 1 or note.pitch < minimum:
            continue
        if note.tick not in onsets or note.pitch > onsets[note.tick].pitch:
            onsets[note.tick] = note
    if not onsets:
        raise ValueError("Selected melody track/register contains no pitched notes")
    if len(onsets) > 4096:
        raise ValueError("Melody exceeds 4096 steps")
    events = [(milliseconds(n.tick, tempos, division), n.pitch,
               milliseconds(n.end, tempos, division)) for _, n in sorted(onsets.items())]
    if events[-1][0] > 3600000:
        raise ValueError("Melody exceeds one hour")
    return events


def match_reference(path, reference):
    """The merged eight-voice file needs its original score to isolate the lead."""
    notes, tempos, division = read_midi(path)
    available = defaultdict(list)
    for note in notes:
        available[note.pitch].append(milliseconds(note.tick, tempos, division))
    for start, pitch, _ in reference:
        if not any(abs(start - ms) <= 1 for ms in available[pitch]):
            raise ValueError(f"Melody guide note {pitch} at {start} ms is absent from supplied arrangement")


def arrange(events):
    low, high = min(n for _, n, _ in events), max(n for _, n, _ in events)
    if high - low > 26:
        raise ValueError("Melody exceeds 27 keys; select an isolated melody, never fold individual notes")
    # One transposition for the entire song preserves every melodic interval.
    transpose = 66 - (low + high) // 2
    rows = []
    for i, (ms, pitch, end) in enumerate(events):
        duration = events[i + 1][0] - ms if i + 1 < len(events) else end - ms
        gate = min(end - ms, duration)
        if not 1 <= gate <= duration <= 65535:
            raise ValueError("Melody durations/gates must be positive and fit 16 bits")
        rows.append((pitch + transpose, duration, gate))
    return rows, transpose


def compile_songs(songs):
    timings, arrays, descriptors, reports = {}, [], [], []
    for index, (title, events) in enumerate(songs):
        rows, transpose = arrange(events)
        codes = []
        for pitch, duration, gate in rows:
            timing = timings.setdefault((duration, gate), len(timings))
            if timing >= 2048:
                raise ValueError("Too many distinct melody timings")
            codes.append((timing << 5) | (pitch - 53))
        arrays.append(f"static const uint16_t betis_song_{index}[] = {{\n" +
                      "\n".join("    " + ",".join(str(c) for c in codes[i:i + 16]) + ","
                                for i in range(0, len(codes), 16)) + "\n};\n")
        descriptors.append(f'    {{"{title}", betis_song_{index}, {len(rows)}}},')
        reports.append({"title": title, "steps": len(rows), "transpose": transpose,
                        "range": [min(n for n, _, _ in rows), max(n for n, _, _ in rows)],
                        "duration_ms": sum(d for _, d, _ in rows),
                        "interval_errors": sum(b[0] - a[0] != y[1] - x[1]
                                               for a, b, x, y in zip(rows, rows[1:], events, events[1:]))})
    text = ("/* User-supplied arrangements; redistribution authorized by the user.\n"
            " * Generated by tools/Compile-Hymn.py; see THIRD_PARTY.md. */\n"
            f"#define BETIS_SONG_TOTAL {len(songs)}U\n"
            "static const struct betis_song_timing betis_song_timings[] = {\n" +
            "\n".join(f"    {{{duration}, {gate}}}," for duration, gate in timings) + "\n};\n" +
            "".join(arrays) + "static const struct betis_song_data betis_songs[] = {\n" +
            "\n".join(descriptors) + "\n};\n")
    return text, reports


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("midi", type=Path)
    parser.add_argument("--track", type=int, default=1, help="1-based melody track")
    parser.add_argument("--min-note", type=int, default=84, help="Centenary lead register; excludes accompaniment")
    parser.add_argument("--charanga", type=Path, required=True, help="Merged FM1 arrangement")
    parser.add_argument("--charanga-melody", type=Path, required=True, help="Original multitrack score; lead on track 1")
    parser.add_argument("--output", type=Path, required=True)
    args = parser.parse_args()
    lead = melody(args.charanga_melody)
    match_reference(args.charanga, lead)
    text, reports = compile_songs([
        ("CENTENARIO", melody(args.midi, args.track, args.min_note)),
        ("NO PUEDO VIVIR SIN TI", lead),
    ])
    args.output.parent.mkdir(parents=True, exist_ok=True)
    args.output.write_text(text, encoding="ascii", newline="\n")
    print(json.dumps({"songs": reports, "input_sha256": {
        p.name: hashlib.sha256(p.read_bytes()).hexdigest()
        for p in (args.midi, args.charanga, args.charanga_melody)}}))


if __name__ == "__main__":
    main()
