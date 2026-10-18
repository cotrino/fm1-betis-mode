#ifndef BETIS_MODE_H
#define BETIS_MODE_H

#include <stdbool.h>
#include <stdint.h>

#define BETIS_PREVIEW_WIDTH 128
#define BETIS_PREVIEW_HEIGHT 64
#define BETIS_PREVIEW_BUFFER_SIZE (BETIS_PREVIEW_WIDTH * BETIS_PREVIEW_HEIGHT / 8)

typedef struct {
    uint8_t pasion_drive;
    uint8_t ole_reverb;
    uint8_t duende_chorus;
    uint16_t bpm;
    bool arpeggiator_active;
    uint8_t last_note;
    uint32_t note_flash_until_ms;
} BetisState;

extern uint8_t betis_framebuffer[BETIS_PREVIEW_BUFFER_SIZE];

void betis_init(void);
const BetisState *betis_get_state(void);
void betis_set_knob(uint8_t knob_id, uint8_t value);
void betis_set_bpm(uint16_t bpm);
void betis_set_arpeggiator(bool enabled);
uint8_t betis_map_midi_note(uint8_t note);
void betis_note_triggered(uint8_t note, uint32_t now_ms);
void betis_render(uint32_t now_ms);
void betis_draw_bitmap(int16_t x, int16_t y, const uint8_t *bitmap,
                       int16_t width, int16_t height);

#endif
