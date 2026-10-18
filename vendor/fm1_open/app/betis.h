/* SPDX-License-Identifier: GPL-3.0-only WITH GPL-3.0-linking-exception */
/* Betis Mode additions, 2026-10-18. */
#ifndef FM1_BETIS_H
#define FM1_BETIS_H
#include "instrument.h"
#define FM1_BETIS_PAGE 19U
#define BETIS_RHYTHMS 4U
struct betis_player { const char *name, *role, *nickname; };
extern const struct betis_player betis_eleven[11];
extern const char *const betis_rhythm_names[BETIS_RHYTHMS];
int betis_rhythm_apply(unsigned rhythm);
int betis_midi_prepare(unsigned enabled);
unsigned betis_song_count(void);
unsigned betis_song_position(void);
unsigned betis_song_note(void);
unsigned betis_song_mode(void);
unsigned betis_song_errors(void);
int betis_song_start(unsigned mode, uint32_t now);
int betis_song_stop(void);
int betis_song_tick(uint32_t now);
void betis_song_press(unsigned note);
int betis_song_seek(int delta);
int betis_init(void);
int betis_scale_set(unsigned enabled);
unsigned betis_scale_enabled(void);
int betis_tuning_cents(unsigned track, unsigned pitch);
#endif
