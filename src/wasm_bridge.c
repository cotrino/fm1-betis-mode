#include <stdint.h>

#include "audio_synth.h"
#include "betis_mode.h"

#ifdef __EMSCRIPTEN__
#include <emscripten/emscripten.h>
#define BETIS_EXPORT EMSCRIPTEN_KEEPALIVE
#else
#define BETIS_EXPORT
#endif

BETIS_EXPORT uint8_t *betis_wasm_get_framebuffer(void) {
    return betis_framebuffer;
}

BETIS_EXPORT void betis_wasm_init(void) {
    betis_init();
    betis_audio_init(BETIS_AUDIO_SAMPLE_RATE);
    betis_audio_set_arpeggiator(betis_get_state()->arpeggiator_active,
                                 betis_get_state()->bpm);
}

BETIS_EXPORT void betis_wasm_set_sample_rate(uint32_t rate) {
    betis_audio_init(rate);
    betis_audio_set_arpeggiator(betis_get_state()->arpeggiator_active,
                                 betis_get_state()->bpm);
    const BetisState *state = betis_get_state();
    const BetisAudioParameters audio_parameters = {
        state->pasion_drive, state->ole_reverb, state->duende_chorus
    };
    betis_audio_set_parameters(audio_parameters);
}

BETIS_EXPORT void betis_wasm_tick(uint32_t now_ms) {
    betis_render(now_ms);
    betis_audio_tick(now_ms);
}

BETIS_EXPORT void betis_wasm_set_knob(uint8_t knob_id, uint8_t value) {
    betis_set_knob(knob_id, value);
    const BetisState *state = betis_get_state();
    const BetisAudioParameters audio_parameters = {
        state->pasion_drive, state->ole_reverb, state->duende_chorus
    };
    betis_audio_set_parameters(audio_parameters);
}

BETIS_EXPORT void betis_wasm_set_bpm(uint16_t bpm) {
    betis_set_bpm(bpm);
    betis_audio_set_arpeggiator(betis_get_state()->arpeggiator_active, bpm);
}

BETIS_EXPORT void betis_wasm_set_arpeggiator(uint8_t enabled) {
    betis_set_arpeggiator(enabled != 0);
    betis_audio_set_arpeggiator(betis_get_state()->arpeggiator_active,
                                 betis_get_state()->bpm);
}

BETIS_EXPORT void betis_wasm_note_on(uint8_t note, uint8_t velocity,
                                      uint32_t now_ms) {
    betis_note_triggered(note, now_ms);
    betis_audio_arpeggiator_note_on(note, velocity);
}

BETIS_EXPORT void betis_wasm_note_off(uint8_t note) {
    betis_audio_arpeggiator_note_off(note);
}

BETIS_EXPORT void betis_wasm_all_notes_off(void) {
    betis_audio_all_notes_off();
}

BETIS_EXPORT void betis_wasm_render_audio(float *output, uint32_t frames) {
    betis_audio_render(output, frames);
}
