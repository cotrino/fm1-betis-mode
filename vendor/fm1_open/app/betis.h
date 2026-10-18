/* SPDX-License-Identifier: GPL-3.0-only WITH GPL-3.0-linking-exception */
/* Betis Mode additions, 2026-10-18. */
#ifndef FM1_BETIS_H
#define FM1_BETIS_H
#include "instrument.h"
#define FM1_BETIS_PAGE 19U
int betis_init(void);
int betis_scale_set(unsigned enabled);
unsigned betis_scale_enabled(void);
int betis_tuning_cents(unsigned track, unsigned pitch);
#endif
