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
int betis_init(void);
int betis_scale_set(unsigned enabled);
unsigned betis_scale_enabled(void);
int betis_tuning_cents(unsigned track, unsigned pitch);
#endif
