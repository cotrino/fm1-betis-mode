#ifndef AUDIO_SYNTH_H
#define AUDIO_SYNTH_H

#include <stdbool.h>
#include <stdint.h>

#define BETIS_AUDIO_SAMPLE_RATE 44100
#define BETIS_AUDIO_MAX_VOICES 8

typedef struct {
    uint8_t drive;
    uint8_t reverb;
    uint8_t chorus;
} BetisAudioParameters;

void betis_audio_init(uint32_t sample_rate);
void betis_audio_set_parameters(BetisAudioParameters parameters);
void betis_audio_set_arpeggiator(bool enabled, uint16_t bpm);
void betis_audio_arpeggiator_note_on(uint8_t midi_note, uint8_t velocity);
void betis_audio_arpeggiator_note_off(uint8_t midi_note);
void betis_audio_tick(uint32_t now_ms);
void betis_audio_note_on(uint8_t midi_note, uint8_t velocity);
void betis_audio_note_off(uint8_t midi_note);
void betis_audio_all_notes_off(void);
void betis_audio_render(float *stereo_interleaved, uint32_t frames);

#endif
