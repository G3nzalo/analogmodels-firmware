/*
 * modelPresets.h
 *
 * The voice settings each 808/909 instrument (see AnalogModels.h) is made
 * for. Choosing an instrument as a voice's waveform in the menu applies them,
 * so the instrument sounds right straight away: original tuning, its own
 * decay, the amp EG as a gate, and pitch EG, FM, transient, filter, drive and
 * the tube saturator off. Loading a kit or a parameter lock does not apply
 * them.
 *
 * Part of a custom firmware for the Erica Synths LXR-02 (not an official
 * Erica Synths release). Added 2026.
 * SPDX-License-Identifier: GPL-3.0-only
 *
 * Licensed under the GNU General Public License version 3, with the
 * additional permission and terms in NOTICE.md. See LICENSE and NOTICE.md in
 * the root of the repository.
 */

#ifndef MODELPRESETS_H_
#define MODELPRESETS_H_

#include <stdint.h>

#define MODEL_PRESET_NO_VOICE 0xff

/** sets one sound parameter (front panel numbering, 0..127) */
typedef void (*ModelPresetSetter)(uint16_t parNr, uint8_t value);

/** the voice (0..5) whose oscillator waveform is parNr, or MODEL_PRESET_NO_VOICE */
uint8_t modelPreset_voiceOfWaveParam(uint16_t parNr);

/** calls set() for every voice parameter the instrument wants. Does nothing
 * for the original waveforms. */
void modelPreset_apply(uint8_t voice, uint8_t waveform, ModelPresetSetter set);

#endif /* MODELPRESETS_H_ */
