#include "audio_synth.h"

#include <math.h>
#include <stddef.h>
#include <string.h>

#include "betis_mode.h"

#define BETIS_PI 3.14159265358979323846
#define REVERB_BUFFER_SIZE 8192
#define CHORUS_BUFFER_SIZE 2048

typedef struct {
    double carrier_phase;
    double modulator_phase;
    double carrier_increment;
    double modulator_increment;
    float envelope;
    uint8_t note;
    uint8_t velocity;
    uint8_t active;
    uint8_t releasing;
} BetisVoice;

static BetisVoice voices[BETIS_AUDIO_MAX_VOICES];
static float reverb_left[REVERB_BUFFER_SIZE];
static float reverb_right[REVERB_BUFFER_SIZE];
static float chorus_left[CHORUS_BUFFER_SIZE];
static float chorus_right[CHORUS_BUFFER_SIZE];
static uint8_t held_notes[BETIS_AUDIO_MAX_VOICES];
static uint8_t held_velocities[BETIS_AUDIO_MAX_VOICES];
static uint8_t held_count;
static uint8_t arpeggiator_index;
static uint8_t arpeggiator_enabled;
static uint8_t arpeggiator_note_active;
static uint32_t sample_rate = BETIS_AUDIO_SAMPLE_RATE;
static uint32_t reverb_position;
static uint32_t chorus_position;
static uint32_t lfo_position;
static uint32_t next_arp_step_ms;
static uint32_t arp_step_ms = 125;
static uint8_t current_arp_note;
static BetisAudioParameters parameters;

void betis_audio_init(uint32_t rate) {
    sample_rate = rate < 8000 ? BETIS_AUDIO_SAMPLE_RATE : rate;
    reverb_position = 0;
    chorus_position = 0;
    lfo_position = 0;
    memset(voices, 0, sizeof(voices));
    memset(held_notes, 0, sizeof(held_notes));
    memset(held_velocities, 0, sizeof(held_velocities));
    memset(reverb_left, 0, sizeof(reverb_left));
    memset(reverb_right, 0, sizeof(reverb_right));
    memset(chorus_left, 0, sizeof(chorus_left));
    memset(chorus_right, 0, sizeof(chorus_right));
    held_count = 0;
    arpeggiator_index = 0;
    arpeggiator_enabled = 0;
    arpeggiator_note_active = 0;
    next_arp_step_ms = 0;
    arp_step_ms = 125;
    parameters.drive = 64;
    parameters.reverb = 48;
    parameters.chorus = 32;
}

void betis_audio_set_parameters(BetisAudioParameters value) {
    parameters = value;
}

void betis_audio_set_arpeggiator(bool enabled, uint16_t bpm) {
    const uint8_t was_enabled = arpeggiator_enabled;
    arpeggiator_enabled = enabled ? 1 : 0;
    if (bpm < 60) {
        bpm = 60;
    } else if (bpm > 180) {
        bpm = 180;
    }
    arp_step_ms = 60000u / ((uint32_t)bpm * 4u);
    if (arp_step_ms == 0) {
        arp_step_ms = 1;
    }
    if (was_enabled && !arpeggiator_enabled) {
        if (arpeggiator_note_active) {
            betis_audio_note_off(current_arp_note);
        }
        arpeggiator_note_active = 0;
        held_count = 0;
    }
}

void betis_audio_arpeggiator_note_on(uint8_t midi_note, uint8_t velocity) {
    if (!arpeggiator_enabled) {
        betis_audio_note_on(midi_note, velocity);
        return;
    }
    if (velocity == 0) {
        betis_audio_arpeggiator_note_off(midi_note);
        return;
    }

    for (uint8_t i = 0; i < held_count; ++i) {
        if (held_notes[i] == midi_note) {
            held_velocities[i] = velocity;
            return;
        }
    }
    if (held_count < BETIS_AUDIO_MAX_VOICES) {
        held_notes[held_count] = midi_note;
        held_velocities[held_count] = velocity;
        ++held_count;
    }
}

void betis_audio_arpeggiator_note_off(uint8_t midi_note) {
    if (!arpeggiator_enabled) {
        betis_audio_note_off(midi_note);
        return;
    }

    for (uint8_t i = 0; i < held_count; ++i) {
        if (held_notes[i] == midi_note) {
            if (arpeggiator_note_active &&
                betis_map_midi_note(midi_note) == current_arp_note) {
                betis_audio_note_off(current_arp_note);
                arpeggiator_note_active = 0;
            }
            for (uint8_t move = i; move + 1 < held_count; ++move) {
                held_notes[move] = held_notes[move + 1];
                held_velocities[move] = held_velocities[move + 1];
            }
            --held_count;
            if (held_count == 0) {
                arpeggiator_index = 0;
            } else if (arpeggiator_index >= held_count) {
                arpeggiator_index = 0;
            }
            return;
        }
    }
}

void betis_audio_tick(uint32_t now_ms) {
    if (!arpeggiator_enabled || held_count == 0 ||
        (int32_t)(now_ms - next_arp_step_ms) < 0) {
        return;
    }
    if (arpeggiator_note_active) {
        betis_audio_note_off(current_arp_note);
    }

    const uint8_t note = held_notes[arpeggiator_index];
    current_arp_note = betis_map_midi_note(note);
    betis_audio_note_on(note, held_velocities[arpeggiator_index]);
    arpeggiator_note_active = 1;
    arpeggiator_index = (uint8_t)((arpeggiator_index + 1) % held_count);
    next_arp_step_ms = now_ms + arp_step_ms;
}

void betis_audio_note_on(uint8_t midi_note, uint8_t velocity) {
    if (velocity == 0) {
        betis_audio_note_off(betis_map_midi_note(midi_note));
        return;
    }

    const uint8_t note = betis_map_midi_note(midi_note);
    BetisVoice *voice = NULL;
    for (uint8_t i = 0; i < BETIS_AUDIO_MAX_VOICES; ++i) {
        if (!voices[i].active) {
            voice = &voices[i];
            break;
        }
    }
    if (voice == NULL) {
        voice = &voices[0];
        for (uint8_t i = 1; i < BETIS_AUDIO_MAX_VOICES; ++i) {
            if (voices[i].envelope < voice->envelope) {
                voice = &voices[i];
            }
        }
    }

    memset(voice, 0, sizeof(*voice));
    voice->active = 1;
    voice->note = note;
    voice->velocity = velocity;
    const double frequency = 440.0 * pow(2.0, ((double)note - 69.0) / 12.0);
    voice->carrier_increment = 2.0 * BETIS_PI * frequency / sample_rate;
    voice->modulator_increment = 2.0 * voice->carrier_increment;
}

void betis_audio_note_off(uint8_t midi_note) {
    const uint8_t note = betis_map_midi_note(midi_note);
    for (uint8_t i = 0; i < BETIS_AUDIO_MAX_VOICES; ++i) {
        if (voices[i].active && voices[i].note == note) {
            voices[i].releasing = 1;
        }
    }
}

void betis_audio_all_notes_off(void) {
    for (uint8_t i = 0; i < BETIS_AUDIO_MAX_VOICES; ++i) {
        voices[i].releasing = 1;
    }
    held_count = 0;
    arpeggiator_note_active = 0;
}

void betis_audio_render(float *output, uint32_t frames) {
    if (output == NULL) {
        return;
    }

    const float attack_step = 1.0f / ((float)sample_rate * 0.008f);
    const float release_step = 1.0f / ((float)sample_rate * 0.12f);
    const float drive = 0.65f + ((float)parameters.drive / 127.0f) * 5.0f;
    const float reverb = ((float)parameters.reverb / 127.0f) * 0.62f;
    const float chorus = ((float)parameters.chorus / 127.0f) * 0.45f;
    const double modulation_index = 0.5 + ((double)parameters.drive / 127.0) * 5.0;

    for (uint32_t frame = 0; frame < frames; ++frame) {
        float dry = 0.0f;
        for (uint8_t i = 0; i < BETIS_AUDIO_MAX_VOICES; ++i) {
            BetisVoice *voice = &voices[i];
            if (!voice->active) {
                continue;
            }

            const double modulator = sin(voice->modulator_phase);
            const double carrier = sin(voice->carrier_phase + modulator * modulation_index);
            voice->modulator_phase += voice->modulator_increment;
            voice->carrier_phase += voice->carrier_increment;
            if (voice->modulator_phase >= 2.0 * BETIS_PI) {
                voice->modulator_phase -= 2.0 * BETIS_PI;
            }
            if (voice->carrier_phase >= 2.0 * BETIS_PI) {
                voice->carrier_phase -= 2.0 * BETIS_PI;
            }

            if (voice->releasing) {
                voice->envelope -= release_step;
                if (voice->envelope <= 0.0f) {
                    voice->envelope = 0.0f;
                    voice->active = 0;
                    continue;
                }
            } else if (voice->envelope < 1.0f) {
                voice->envelope += attack_step;
                if (voice->envelope > 1.0f) {
                    voice->envelope = 1.0f;
                }
            }

            dry += (float)carrier * voice->envelope *
                   ((float)voice->velocity / 127.0f) * 0.12f;
        }

        const float saturated = tanhf(dry * drive);
        const uint32_t delayed = (reverb_position + REVERB_BUFFER_SIZE - 4096) %
                                 REVERB_BUFFER_SIZE;
        const float wet_left = reverb_left[delayed];
        const float wet_right = reverb_right[delayed];
        const double lfo_phase = 2.0 * BETIS_PI * lfo_position / sample_rate;
        const uint32_t chorus_delay_left = 441u +
            (uint32_t)((sin(lfo_phase) + 1.0) * 110.0);
        const uint32_t chorus_delay_right = 441u +
            (uint32_t)((sin(lfo_phase + BETIS_PI / 2.0) + 1.0) * 110.0);
        const uint32_t chorus_read_left =
            (chorus_position + CHORUS_BUFFER_SIZE - chorus_delay_left) &
            (CHORUS_BUFFER_SIZE - 1);
        const uint32_t chorus_read_right =
            (chorus_position + CHORUS_BUFFER_SIZE - chorus_delay_right) &
            (CHORUS_BUFFER_SIZE - 1);
        const float left = saturated + wet_left * reverb +
                           chorus_left[chorus_read_left] * chorus;
        const float right = saturated + wet_right * reverb +
                            chorus_right[chorus_read_right] * chorus;

        reverb_left[reverb_position] = saturated + wet_right * reverb * 0.55f;
        reverb_right[reverb_position] = saturated + wet_left * reverb * 0.55f;
        chorus_left[chorus_position] = saturated;
        chorus_right[chorus_position] = saturated;
        output[frame * 2] = tanhf(left);
        output[frame * 2 + 1] = tanhf(right);

        reverb_position = (reverb_position + 1) % REVERB_BUFFER_SIZE;
        chorus_position = (chorus_position + 1) & (CHORUS_BUFFER_SIZE - 1);
        ++lfo_position;
        if (lfo_position >= sample_rate) {
            lfo_position = 0;
        }
    }
}
