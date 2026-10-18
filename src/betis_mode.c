#include "betis_mode.h"

#include <string.h>

#include "betis_bitmaps.h"

uint8_t betis_framebuffer[BETIS_PREVIEW_BUFFER_SIZE];

static BetisState state;

static void draw_pixel(int16_t x, int16_t y) {
    if (x < 0 || x >= BETIS_PREVIEW_WIDTH || y < 0 || y >= BETIS_PREVIEW_HEIGHT) {
        return;
    }

    const uint16_t index = (uint16_t)x + (uint16_t)(y / 8) * BETIS_PREVIEW_WIDTH;
    betis_framebuffer[index] |= (uint8_t)(1u << (y % 8));
}

static void draw_number(uint8_t value, int16_t x, int16_t y) {
    static const uint8_t digits[10][5] = {
        {0x07, 0x05, 0x05, 0x05, 0x07}, {0x02, 0x06, 0x02, 0x02, 0x07},
        {0x07, 0x01, 0x07, 0x04, 0x07}, {0x07, 0x01, 0x07, 0x01, 0x07},
        {0x05, 0x05, 0x07, 0x01, 0x01}, {0x07, 0x04, 0x07, 0x01, 0x07},
        {0x07, 0x04, 0x07, 0x05, 0x07}, {0x07, 0x01, 0x01, 0x01, 0x01},
        {0x07, 0x05, 0x07, 0x05, 0x07}, {0x07, 0x05, 0x07, 0x01, 0x07}
    };
    const uint8_t hundreds = (uint8_t)(value / 100);
    const uint8_t tens = (uint8_t)((value / 10) % 10);
    const uint8_t ones = (uint8_t)(value % 10);
    const uint8_t numbers[3] = {hundreds, tens, ones};
    uint8_t first = 0;

    while (first < 2 && numbers[first] == 0) {
        ++first;
    }

    for (uint8_t digit = first; digit < 3; ++digit) {
        for (uint8_t row = 0; row < 5; ++row) {
            for (uint8_t col = 0; col < 3; ++col) {
                if (digits[numbers[digit]][row] & (uint8_t)(1u << (2 - col))) {
                    draw_pixel((int16_t)(x + (digit - first) * 5 + col),
                               (int16_t)(y + row));
                }
            }
        }
    }
}

void betis_init(void) {
    state.pasion_drive = 64;
    state.ole_reverb = 48;
    state.duende_chorus = 32;
    state.bpm = 120;
    state.arpeggiator_active = true;
    state.last_note = 0;
    state.note_flash_until_ms = 0;
    memset(betis_framebuffer, 0, sizeof(betis_framebuffer));
}

const BetisState *betis_get_state(void) {
    return &state;
}

void betis_set_knob(uint8_t knob_id, uint8_t value) {
    switch (knob_id) {
        case 0:
            state.pasion_drive = value;
            break;
        case 1:
            state.ole_reverb = value;
            break;
        case 2:
            state.duende_chorus = value;
            break;
        default:
            break;
    }
}

void betis_set_bpm(uint16_t bpm) {
    if (bpm < 60) {
        bpm = 60;
    } else if (bpm > 180) {
        bpm = 180;
    }
    state.bpm = bpm;
}

void betis_set_arpeggiator(bool enabled) {
    state.arpeggiator_active = enabled;
}

uint8_t betis_map_midi_note(uint8_t note) {
    static const uint8_t e_phrygian_dominant[7] = {4, 5, 8, 9, 11, 0, 2};
    const uint8_t octave = (uint8_t)(note / 12);
    uint8_t best = note;
    uint8_t best_distance = 12;

    for (uint8_t i = 0; i < 7; ++i) {
        const int16_t candidate = (int16_t)(octave * 12 + e_phrygian_dominant[i]);
        if (candidate <= 127) {
            const uint8_t distance = (uint8_t)(candidate > note
                ? candidate - note : note - candidate);
            if (distance < best_distance ||
                (distance == best_distance && candidate < best)) {
                best = (uint8_t)candidate;
                best_distance = distance;
            }
        }
    }

    return best;
}

void betis_note_triggered(uint8_t note, uint32_t now_ms) {
    state.last_note = betis_map_midi_note(note);
    state.note_flash_until_ms = now_ms + 140;
}

void betis_draw_bitmap(int16_t x, int16_t y, const uint8_t *bitmap,
                       int16_t width, int16_t height) {
    if (bitmap == NULL || width <= 0 || height <= 0) {
        return;
    }

    const int16_t row_bytes = (int16_t)((width + 7) / 8);
    for (int16_t row = 0; row < height; ++row) {
        for (int16_t col = 0; col < width; ++col) {
            const uint8_t byte = bitmap[row * row_bytes + col / 8];
            if (byte & (uint8_t)(0x80u >> (col % 8))) {
                draw_pixel((int16_t)(x + col), (int16_t)(y + row));
            }
        }
    }
}

void betis_render(uint32_t now_ms) {
    memset(betis_framebuffer, 0, sizeof(betis_framebuffer));
    betis_draw_bitmap(4, 4, betis_shield_24x24,
                      BETIS_SHIELD_WIDTH, BETIS_SHIELD_HEIGHT);

    const int16_t bounce = (int16_t)((now_ms / (state.bpm > 130 ? 90 : 120)) % 5);
    betis_draw_bitmap(96, (int16_t)(4 + bounce), palmerin_24x24,
                      PALMERIN_WIDTH, PALMERIN_HEIGHT);

    draw_number((uint8_t)state.bpm, 42, 12);
    for (uint8_t knob = 0; knob < 3; ++knob) {
        const uint8_t value = knob == 0 ? state.pasion_drive
            : (knob == 1 ? state.ole_reverb : state.duende_chorus);
        const int16_t y = (int16_t)(43 + knob * 6);
        const int16_t width = (int16_t)(value / 2);
        for (int16_t x = 4; x < 124; ++x) {
            draw_pixel(x, y);
        }
        for (int16_t x = 4; x < 4 + width; ++x) {
            draw_pixel(x, (int16_t)(y - 1));
            draw_pixel(x, (int16_t)(y + 1));
        }
    }

    if ((int32_t)(state.note_flash_until_ms - now_ms) > 0) {
        for (int16_t x = 36; x < 92; ++x) {
            draw_pixel(x, 34);
            draw_pixel(x, 38);
        }
        for (int16_t y = 34; y <= 38; ++y) {
            draw_pixel(36, y);
            draw_pixel(91, y);
        }
        draw_number(state.last_note, 58, 34);
    }
}
