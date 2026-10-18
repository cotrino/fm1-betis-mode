/* SPDX-License-Identifier: GPL-3.0-only WITH GPL-3.0-linking-exception */
/* Betis Mode additions, 2026-10-18. Uses the official engine on both targets. */
#include "betis.h"
#include <string.h>

/* E Phrygian dominant. Shift by at most a semitone, retaining original
 * MIDI note identities so adjacent quantized keys release independently. */
static const int8_t scale[12] = {0, -100, 0, -100, 0, 0, -100, 100, 0, 0, -100, 0};

/* Starting XI, Copa final 23 April 2022, not a current-team prediction. */
const struct betis_player betis_eleven[11] = {
    {"CLAUDIO BRAVO", "PORTERO", "EL MURO"},
    {"HECTOR BELLERIN", "LATERAL DERECHO", "CENTRO PARA EL PANDA"},
    {"GERMAN PEZZELLA", "CENTRAL", "CERROJO VERDIBLANCO"},
    {"MARC BARTRA", "CENTRAL", "SALIDA CON ARTE"},
    {"ALEX MORENO", "LATERAL IZQUIERDO", "POR LA BANDA"},
    {"GUIDO RODRIGUEZ", "MEDIOCENTRO", "AQUI NO PASA NADIE"},
    {"WILLIAM CARVALHO", "MEDIOCENTRO", "CALMA Y COMPAS"},
    {"SERGIO CANALES", "MEDIAPUNTA", "ENTRE LINEAS"},
    {"NABIL FEKIR", "MEDIAPUNTA", "MAGIA EN LA BOTA"},
    {"JUANMI", "EXTREMO", "AL DESMARQUE"},
    {"BORJA IGLESIAS", "DELANTERO", "EL PANDA"}
};
const char *const betis_rhythm_names[BETIS_RHYTHMS] = {
    "PALMAS DEL HELIOPOLIS", "BOMBO DE LAS 13 BARRAS",
    "GOL DEL PANDA", "MANQUE PIERDA"
};
static struct groove_tuning midi_tuning;
static unsigned midi_prepared, midi_arp;
#ifdef __wasm__
__attribute__((export_name("betis_midi_prepare")))
#endif
int betis_midi_prepare(unsigned enabled)
{
    if (enabled && !midi_prepared) {
        if (groove_source_type(0) != GROOVE_DX7) return -1;
        groove_tuning_get(0, &midi_tuning);
        struct groove_tuning equal = {{0}, {0}};
        if (groove_tuning_set(0, &equal)) return -1;
        midi_arp = groove_arp_enabled(0);
        groove_arp_set(0, 0);
        midi_prepared = 1;
    } else if (!enabled && midi_prepared) {
        if (groove_tuning_set(0, &midi_tuning)) return -1;
        groove_arp_set(0, midi_arp);
        midi_prepared = 0;
    }
    return 0;
}
int betis_rhythm_apply(unsigned rhythm)
{
    /* Only an explicit EDIT installs a pattern in a user's project.
     * Refuse a changed track source rather than replacing its instrument. */
    if (rhythm >= BETIS_RHYTHMS || groove_source_type(7) != GROOVE_808) return -1;
    static const uint16_t hits[BETIS_RHYTHMS][3] = {
        {0x1111, 0x1010, 0x5454},
        {0x4141, 0x1414, 0xaaaa},
        {0x0505, 0xc850, 0xffff},
        {0x0909, 0x9090, 0x5252}
    };
    static const unsigned pads[3] = {0, 6, 10};
    groove_sequence_length_set(7, 16);
    for (unsigned s = 0; s < 16; ++s) {
        for (unsigned p = 0; p < GROOVE_DRUMS; ++p)
            groove_sequence_edit(7, s, groove_pad_note(7, p), 0);
        for (unsigned p = 0; p < 3; ++p)
            if (hits[rhythm][p] & (1U << s))
                groove_sequence_edit(7, s, groove_pad_note(7, pads[p]), p == 2 ? 72 : 108);
    }
    return 0;
}
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
    midi_prepared = 0;
    if (betis_scale_set(1)) return -1;
    groove_arp_set(0, 1);
    groove_sends_edit(0, 0, 5);
    groove_sends_edit(0, 1, 3);
    /* Original synthesized 808 crowd rhythm, no recorded chants or samples.
     * It is RAM-only until the user explicitly saves a project. */
    return betis_rhythm_apply(0);
}
