/* SPDX-License-Identifier: GPL-3.0-only WITH GPL-3.0-linking-exception */
/* Betis Mode additions, 2026-10-18. Uses the official engine on both targets. */
#include "betis.h"
#include <string.h>

/* E Phrygian dominant. Shift by at most a semitone, retaining original
 * MIDI note identities so adjacent quantized keys release independently. */
static const int8_t scale[12] = {0, -100, 0, -100, 0, 0, -100, 100, 0, 0, -100, 0};

int betis_scale_set(unsigned enabled)
{
    struct groove_tuning tables[GROOVE_TRACKS];
    if (groove_tuning_capture(tables)) return -1;
    for (unsigned t = 0; t < GROOVE_TRACKS; ++t) {
        if (enabled) memcpy(tables[t].main, scale, sizeof scale);
        else memset(tables[t].main, 0, sizeof scale);
    }
    return groove_tuning_apply(tables);
}
unsigned betis_scale_enabled(void)
{
    struct groove_tuning table;
    for (unsigned t = 0; t < GROOVE_TRACKS; ++t) {
        if (groove_pad_count(t)) continue;
        groove_tuning_get(t, &table);
        if (memcmp(table.main, scale, sizeof scale)) return 0;
    }
    return 1;
}
#ifdef __wasm__
__attribute__((export_name("betis_tuning_cents")))
#endif
int betis_tuning_cents(unsigned track, unsigned pitch)
{
    if (track >= GROOVE_TRACKS || pitch >= 12) return -1000;
    struct groove_tuning table;
    groove_tuning_get(track, &table);
    return table.main[pitch];
}
int betis_init(void)
{
    if (betis_scale_set(1)) return -1;
    groove_arp_set(0, 1);
    groove_sends_edit(0, 0, 5);
    groove_sends_edit(0, 1, 3);
    /* Original synthesized 808 crowd rhythm, no recorded chants or samples.
     * It is RAM-only until the user explicitly saves a project. */
    groove_sequence_length_set(7, 16);
    for (unsigned s = 0; s < 16; s += 4)
        groove_sequence_edit(7, s, groove_pad_note(7, 0), 100);
    for (unsigned s = 4; s < 16; s += 8)
        groove_sequence_edit(7, s, groove_pad_note(7, 2), 108);
    for (unsigned s = 2; s < 16; s += 2)
        groove_sequence_edit(7, s, groove_pad_note(7, 3), 72);
    return 0;
}
