/*
 * AnalogModels.h
 *
 * The instruments of the TR-808 and TR-909, modelled after their circuits,
 * and acoustic instruments (gamelan, hand drums, taiko) modelled after their
 * modes of vibration. They are selected like extra oscillator waveforms (after Sin, Tri, Saw, Rec,
 * Noi and PWM), so every voice keeps its tuning, filter, drive, LFO and FX.
 * Each instrument makes its own pitch sweeps, clicks, noise and envelopes;
 * the voice's amp decay knob sets how long it rings, and the PWM knob becomes
 * a tube saturator ("sat" in the menu).
 *
 * Choosing one of them in the menu also sets the rest of the voice for it
 * (see menu/modelPresets.c): coarse 0 is the original tuning, the amp EG is
 * used as a gate and pitch EG, FM, transient, filter and drive are off.
 *
 * Part of a custom firmware for the Erica Synths LXR-02 (not an official
 * Erica Synths release). Added 2026.
 * SPDX-License-Identifier: GPL-3.0-only
 *
 * Licensed under the GNU General Public License version 3, with the
 * additional permission and terms in NOTICE.md. See LICENSE and NOTICE.md in
 * the root of the repository.
 */

#ifndef ANALOGMODELS_H_
#define ANALOGMODELS_H_

#include "Soundengine/DSPAudio/Oscillator.h"

/* the instruments follow the six original waveforms in the waveform list */
enum AnalogModelEnum
{
	MODEL_BD8 = NUM_OSC_WAVEFORMS,	/* 808 bass drum */
	MODEL_BD9,		/* 909 bass drum */
	MODEL_SD8,		/* 808 snare */
	MODEL_SD9,		/* 909 snare */
	MODEL_LT8,		/* 808 low tom */
	MODEL_MT8,		/* 808 mid tom */
	MODEL_HT8,		/* 808 high tom */
	MODEL_LT9,		/* 909 low tom */
	MODEL_MT9,		/* 909 mid tom */
	MODEL_HT9,		/* 909 high tom */
	MODEL_LC8,		/* 808 low conga */
	MODEL_MC8,		/* 808 mid conga */
	MODEL_HC8,		/* 808 high conga */
	MODEL_RS8,		/* 808 rim shot */
	MODEL_RS9,		/* 909 rim shot */
	MODEL_CL8,		/* 808 claves */
	MODEL_CP8,		/* 808 hand clap */
	MODEL_CP9,		/* 909 hand clap */
	MODEL_MA8,		/* 808 maracas */
	MODEL_CB8,		/* 808 cowbell */
	MODEL_HH8,		/* 808 hi-hat (closed or open by the decay) */
	MODEL_HH9,		/* 909 hi-hat */
	MODEL_CY8,		/* 808 cymbal */
	MODEL_CR9,		/* 909 crash */
	MODEL_RD9,		/* 909 ride */
	MODEL_SRN,		/* gamelan saron */
	MODEL_GND,		/* gamelan gender, a pair with ombak */
	MODEL_BNG,		/* gamelan bonang */
	MODEL_GNG,		/* gamelan gong ageng */
	MODEL_END
};

#define NUM_ANALOG_MODELS (MODEL_END - NUM_OSC_WAVEFORMS)

static inline uint8_t osc_isModel(const uint8_t waveform)
{
	return (waveform >= NUM_OSC_WAVEFORMS) && (waveform < MODEL_END);
}

/** start a new hit. Does nothing if the oscillator does not run a model */
void model_trigger(OscInfo* osc, const uint8_t vel);

/** render one block. sat is the voice's PWM parameter [0:1], the tube saturator */
void model_calcBlock(OscInfo* osc, int16_t* buf, const uint8_t size, const float gain, const float sat);

#endif /* ANALOGMODELS_H_ */
