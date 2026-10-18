/* SPDX-License-Identifier: GPL-3.0-only WITH GPL-3.0-linking-exception */
/* Betis Mode additions, 2026-10-18. Uses the official engine on both targets. */
#include "betis.h"
#include <string.h>

/* E Phrygian dominant. Shift by at most a semitone, retaining original
 * MIDI note identities so adjacent quantized keys release independently. */
static const int8_t scale[12] = {0, -100, 0, -100, 0, 0, -100, 100, 0, 0, -100, 0};

/* Project representative XI, 2025/26 squad; not an official fixed lineup. */
const struct betis_player betis_eleven[11] = {
    {"ALVARO VALLES", "PORTERO", "EL MURO"},
    {"HECTOR BELLERIN", "LATERAL DERECHO", "POR LA BANDA"},
    {"NATAN", "CENTRAL", "CERROJO VERDIBLANCO"},
    {"MARC BARTRA", "CENTRAL", "SALIDA CON ARTE"},
    {"RICARDO RODRIGUEZ", "LATERAL IZQUIERDO", "RECORRIDO Y COMPAS"},
    {"SOFYAN AMRABAT", "MEDIOCENTRO", "AQUI NO PASA NADIE"},
    {"PABLO FORNALS", "MEDIOCENTRO", "PASE CON ARTE"},
    {"ISCO", "MEDIAPUNTA", "MAGIA EN LA BOTA"},
    {"ANTONY", "EXTREMO DERECHO", "DESBORDE VERDIBLANCO"},
    {"EZ ABDE", "EXTREMO IZQUIERDO", "AL DESMARQUE"},
    {"CUCHO HERNANDEZ", "DELANTERO", "AL FONDO DE LA RED"}
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
struct betis_song_timing { uint16_t duration, gate; };
struct betis_song_data { const char *title; const uint16_t *steps; unsigned count; };
#if __has_include("betis_song_data.h")
#include "betis_song_data.h"
#else
#define BETIS_SONG_TOTAL 0U
static const struct betis_song_timing betis_song_timings[1] = {{600, 600}};
static const struct betis_song_data betis_songs[1] = {{"SIN CANCIONES", NULL, 0}};
#endif
static unsigned song_selected, song_position, song_mode, song_errors, song_sounding = 128;
static uint32_t song_deadline, song_release;
#ifdef __wasm__
#define BETIS_EXPORT(name) __attribute__((export_name(name)))
#else
#define BETIS_EXPORT(name)
#endif
BETIS_EXPORT("betis_song_total") unsigned betis_song_total(void) { return BETIS_SONG_TOTAL; }
BETIS_EXPORT("betis_song_selected") unsigned betis_song_selected(void) { return song_selected; }
const char *betis_song_title(void) { return betis_songs[song_selected].title; }
BETIS_EXPORT("betis_song_count") unsigned betis_song_count(void) { return betis_songs[song_selected].count; }
BETIS_EXPORT("betis_song_position") unsigned betis_song_position(void) { return song_position; }
BETIS_EXPORT("betis_song_mode") unsigned betis_song_mode(void) { return song_mode; }
BETIS_EXPORT("betis_song_errors") unsigned betis_song_errors(void) { return song_errors; }
BETIS_EXPORT("betis_song_note") unsigned betis_song_note(void)
{
    return song_position < betis_song_count()
        ? 53U + (betis_songs[song_selected].steps[song_position] & 31U) : 128;
}
BETIS_EXPORT("betis_song_duration") unsigned betis_song_duration(void)
{
    unsigned duration = 0;
    const struct betis_song_data *song = &betis_songs[song_selected];
    for (unsigned i = 0; i < song->count; ++i)
        duration += betis_song_timings[song->steps[i] >> 5].duration;
    return duration;
}
static int song_message(uint8_t status, uint8_t note, uint8_t velocity)
{
    const uint8_t message[] = {status, note, velocity};
    return fm1_midi_post(message, sizeof message);
}
int betis_song_stop(void)
{
    if (song_sounding < 128 && song_message(0x80, song_sounding, 0)) return -1;
    song_sounding = 128;
    song_mode = 0;
    return betis_midi_prepare(0);
}
BETIS_EXPORT("betis_song_select") int betis_song_select(unsigned selected)
{
    if (selected >= BETIS_SONG_TOTAL) return -1;
    if (betis_song_stop()) return -1;
    song_selected = selected;
    song_position = song_errors = 0;
    return 0;
}
int betis_song_start(unsigned mode, uint32_t now)
{
    if (!betis_song_count() || mode < 1 || mode > 2) return -1;
    if (betis_song_stop() || betis_midi_prepare(1)) return -1;
    const uint8_t stop = 0xfc;
    if (fm1_midi_post(&stop, 1)) { (void)betis_midi_prepare(0); return -1; }
    song_position = song_errors = 0;
    song_mode = mode;
    song_deadline = now;
    return 0;
}
int betis_song_tick(uint32_t now)
{
    if (song_mode != 1) return 0;
    if (song_sounding < 128 && (int32_t)(now - song_release) >= 0) {
        if (song_message(0x80, song_sounding, 0)) return -1;
        song_sounding = 128;
    }
    if ((int32_t)(now - song_deadline) < 0) return 0;
    if (song_position >= betis_song_count()) return betis_song_stop();
    unsigned step = betis_songs[song_selected].steps[song_position];
    const struct betis_song_timing *timing = &betis_song_timings[step >> 5];
    unsigned note = 53U + (step & 31U);
    if (song_message(0x90, note, 90)) return -1;
    song_sounding = note;
    song_release = song_deadline + timing->gate;
    song_deadline += timing->duration;
    ++song_position;
    return 0;
}
void betis_song_press(unsigned note)
{
    if (song_mode != 2 || song_position >= betis_song_count()) return;
    if (note == betis_song_note()) ++song_position;
    else if (song_errors < 9999) ++song_errors;
}
int betis_song_seek(int delta)
{
    unsigned count = betis_song_count();
    if (song_mode != 2 || !count) return -1;
    int next = (int)song_position + delta;
    song_position = next < 0 ? 0 : (unsigned)next >= count ? count - 1 : (unsigned)next;
    return 0;
}
struct betis_penalties betis_penalties;
static uint32_t penalty_random = 1907;
BETIS_EXPORT("betis_penalty_state") unsigned betis_penalty_state(void)
{
    const struct betis_penalties *p = &betis_penalties;
    return p->keeper | (p->aim << 1) | (p->opponent << 3) |
        (p->round << 5) | (p->score << 8) | (p->result << 11);
}
void betis_penalty_reset(unsigned keeper)
{
    memset(&betis_penalties, 0, sizeof betis_penalties);
    betis_penalties.keeper = !!keeper;
    betis_penalties.aim = 1;
}
int betis_penalty_shoot(uint32_t now)
{
    struct betis_penalties *p = &betis_penalties;
    if (p->round >= 5) return -1;
    penalty_random = (penalty_random ^ now) * 1664525U + 1013904223U;
    p->opponent = (uint8_t)((penalty_random >> 16) % 3);
    unsigned saved = p->aim == p->opponent;
    p->result = saved ? 2 : 1;
    p->score += p->keeper ? saved : !saved;
    ++p->round;
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
    song_selected = song_position = song_mode = song_errors = 0;
    song_sounding = 128;
    betis_penalty_reset(0);
    if (betis_scale_set(1)) return -1;
    groove_arp_set(0, 1);
    groove_sends_edit(0, 0, 5);
    groove_sends_edit(0, 1, 3);
    /* Original synthesized 808 crowd rhythm, no recorded chants or samples.
     * It is RAM-only until the user explicitly saves a project. */
    return betis_rhythm_apply(0);
}
