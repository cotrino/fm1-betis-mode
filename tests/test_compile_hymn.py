import runpy
import struct
import tempfile
import unittest
from pathlib import Path


ROOT = Path(__file__).resolve().parents[1]
compiler = runpy.run_path(str(ROOT / "tools" / "Compile-Hymn.py"))


def vlq(value):
    result = [value & 127]
    while value >> 7:
        value >>= 7
        result.insert(0, 128 | (value & 127))
    return bytes(result)


def smf(events):
    body, previous = bytearray(), 0
    for tick, message in events:
        body.extend(vlq(tick - previous) + bytes(message))
        previous = tick
    body.extend(b"\0\xff\x2f\0")
    return (b"MThd" + struct.pack(">IHHH", 6, 0, 1, 480) +
            b"MTrk" + struct.pack(">I", len(body)) + body)


class HymnCompilerTests(unittest.TestCase):
    def test_one_transposition_preserves_every_interval(self):
        events = [(i * 500, note, i * 500 + 400)
                  for i, note in enumerate([88, 90, 95, 102, 93, 88])]
        rows, transpose = compiler["arrange"](events)
        self.assertEqual(transpose, -29)
        self.assertEqual([n for n, _, _ in rows], [n + transpose for _, n, _ in events])
        self.assertEqual([b[0] - a[0] for a, b in zip(rows, rows[1:])],
                         [b[1] - a[1] for a, b in zip(events, events[1:])])
        self.assertTrue(all(53 <= n <= 79 for n, _, _ in rows))

    def test_wide_melody_is_rejected_not_folded(self):
        with self.assertRaisesRegex(ValueError, "never fold"):
            compiler["arrange"]([(0, 48, 100), (100, 80, 200)])

    def test_register_filter_excludes_accompaniment_and_keeps_rests(self):
        events = [(0, [0x90, 91, 90]), (120, [0x80, 91, 0]),
                  (240, [0x90, 67, 90]), (360, [0x80, 67, 0]),
                  (480, [0x90, 93, 90]), (600, [0x80, 93, 0])]
        with tempfile.TemporaryDirectory() as folder:
            path = Path(folder) / "score.mid"
            path.write_bytes(smf(events))
            notes = compiler["melody"](path, minimum=84)
        self.assertEqual(notes, [(0, 91, 125), (500, 93, 625)])
        rows, _ = compiler["arrange"](notes)
        self.assertEqual([(duration, gate) for _, duration, gate in rows],
                         [(500, 125), (125, 125)])

    def test_tempo_sustain_and_drums(self):
        events = [(0, [0xff, 0x51, 3, 0x0f, 0x42, 0x40]),
                  (0, [0xb0, 64, 127]), (0, [0x90, 60, 90]),
                  (0, [0x99, 100, 90]), (240, [0x80, 60, 0]),
                  (480, [0xb0, 64, 0])]
        with tempfile.TemporaryDirectory() as folder:
            path = Path(folder) / "score.mid"
            path.write_bytes(smf(events))
            self.assertEqual(compiler["melody"](path), [(0, 60, 1000)])

    def test_missing_reference_note_is_an_error(self):
        with tempfile.TemporaryDirectory() as folder:
            path = Path(folder) / "score.mid"
            path.write_bytes(smf([(0, [0x90, 60, 90]), (480, [0x80, 60, 0])]))
            with self.assertRaisesRegex(ValueError, "absent"):
                compiler["match_reference"](path, [(0, 61, 500)])

    def test_compact_tables_retain_all_notes_and_timings(self):
        events = [(0, 88, 125), (500, 90, 625), (750, 93, 1000)]
        text, reports = compiler["compile_songs"]([("ONE", events), ("TWO", events)])
        self.assertIn("#define BETIS_SONG_TOTAL 2U", text)
        self.assertIn("{500, 125}", text)
        self.assertEqual([r["interval_errors"] for r in reports], [0, 0])
        self.assertEqual([r["steps"] for r in reports], [3, 3])

    def test_supplied_scores_match_published_tables(self):
        folder = ROOT / "himnos"
        if not (folder / "himno-centenario-del-betis.mid").exists():
            self.skipTest("Original user MIDI files are not redistributed")
        first = compiler["melody"](folder / "himno-centenario-del-betis.mid", minimum=84)
        second = compiler["melody"](folder / "no-puedo-vivir-sin-ti-charanga.mid")
        compiler["match_reference"](folder / "no-puedo-vivir-sin-ti-charanga-fm1.mid", second)
        text, reports = compiler["compile_songs"]([
            ("CENTENARIO", first), ("NO PUEDO VIVIR SIN TI", second)])
        self.assertEqual([r["steps"] for r in reports], [193, 355])
        self.assertEqual([r["range"] for r in reports], [[59, 73], [54, 78]])
        self.assertEqual([r["interval_errors"] for r in reports], [0, 0])
        self.assertEqual(text, (ROOT / "vendor" / "fm1_open" / "app" / "betis_song_data.h").read_text())


if __name__ == "__main__":
    unittest.main()
