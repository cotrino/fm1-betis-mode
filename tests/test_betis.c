#include <assert.h>
#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "audio_synth.h"
#include "betis_mode.h"

static int note_is_in_e_phrygian_dominant(uint8_t note) {
    static const uint8_t scale[] = {0, 2, 4, 5, 8, 9, 11};
    const uint8_t pitch_class = (uint8_t)(note % 12);
    for (uint8_t i = 0; i < sizeof(scale); ++i) {
        if (pitch_class == scale[i]) {
            return 1;
        }
    }
    return 0;
}

int main(void) {
    betis_init();
    assert(betis_get_state()->bpm == 120);
    betis_set_bpm(20);
    assert(betis_get_state()->bpm == 60);
    betis_set_bpm(220);
    assert(betis_get_state()->bpm == 180);

    for (uint16_t note = 0; note < 128; ++note) {
        const uint8_t mapped = betis_map_midi_note((uint8_t)note);
        assert(mapped <= 127);
        assert(note_is_in_e_phrygian_dominant(mapped));
    }

    betis_set_knob(0, 100);
    assert(betis_get_state()->pasion_drive == 100);
    betis_set_knob(3, 22);
    assert(betis_get_state()->pasion_drive == 100);

    betis_render(1000);
    int has_pixels = 0;
    for (size_t i = 0; i < sizeof(betis_framebuffer); ++i) {
        has_pixels |= betis_framebuffer[i] != 0;
    }
    assert(has_pixels);

    betis_render(1050);
    uint8_t baseline[BETIS_PREVIEW_BUFFER_SIZE];
    memcpy(baseline, betis_framebuffer, sizeof(baseline));
    betis_note_triggered(60, 1050);
    betis_render(1050);
    assert(memcmp(betis_framebuffer, baseline, sizeof(baseline)) != 0);
    assert(betis_get_state()->last_note == betis_map_midi_note(60));
    assert(betis_get_state()->note_flash_until_ms == 1190);

    betis_audio_init(BETIS_AUDIO_SAMPLE_RATE);
    betis_audio_note_on(60, 100);
    float samples[512 * 2];
    memset(samples, 0, sizeof(samples));
    betis_audio_render(samples, 512);
    int has_audio = 0;
    for (size_t i = 0; i < sizeof(samples) / sizeof(samples[0]); ++i) {
        assert(isfinite(samples[i]));
        has_audio |= fabsf(samples[i]) > 0.0001f;
    }
    assert(has_audio);
    betis_audio_all_notes_off();

    betis_audio_init(BETIS_AUDIO_SAMPLE_RATE);
    betis_audio_set_arpeggiator(true, 120);
    betis_audio_arpeggiator_note_on(60, 100);
    betis_audio_arpeggiator_note_on(64, 100);
    betis_audio_tick(2000);
    memset(samples, 0, sizeof(samples));
    betis_audio_render(samples, 512);
    has_audio = 0;
    for (size_t i = 0; i < sizeof(samples) / sizeof(samples[0]); ++i) {
        has_audio |= fabsf(samples[i]) > 0.0001f;
    }
    assert(has_audio);
    betis_audio_arpeggiator_note_off(60);
    betis_audio_all_notes_off();
    puts("Betis mode core tests passed.");
    return 0;
}
