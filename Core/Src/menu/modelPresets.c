/*
 * modelPresets.c
 *
 * The voice settings of the 808/909 instruments, see modelPresets.h.
 *
 * Part of a custom firmware for the Erica Synths LXR-02 (not an official
 * Erica Synths release). Added 2026.
 * SPDX-License-Identifier: GPL-3.0-only
 *
 * Licensed under the GNU General Public License version 3, with the
 * additional permission and terms in NOTICE.md. See LICENSE and NOTICE.md in
 * the root of the repository.
 */

#include "menu/modelPresets.h"
#include "Parameters.h"
#include "Soundengine/DSPAudio/AnalogModels.h"

#define TUNING_ORIGINAL	60		// coarse 0 on the display: the instrument's original pitch
#define FINE_CENTER		63
#define SLOPE_GATE		127		// amp EG stays open until it closes at the end
#define FILTER_OFF		7

/* amp decay knob values. The instrument has faded to -60 dB when the amp
 * gate closes, so these give the original decays (time constant in ms in
 * the comments; one knob step is about 12 ms of gate at the short end).
 * decayOpen is the open hi-hat decay on the hi-hat voice. */
typedef struct { uint8_t decay, decayOpen; } ModelPreset;

static const ModelPreset modelPresets[NUM_ANALOG_MODELS] =
{
	{ 65,  0 },	// BD8 220 ms
	{ 47,  0 },	// BD9 120 ms after a 10 ms hold
	{ 26,  0 },	// SD8  55 ms snappy
	{ 32,  0 },	// SD9  70 ms snappy
	{ 44,  0 },	// LT8 110 ms
	{ 37,  0 },	// MT8  85 ms
	{ 32,  0 },	// HT8  70 ms
	{ 51,  0 },	// LT9 140 ms
	{ 46,  0 },	// MT9 120 ms
	{ 41,  0 },	// HT9 100 ms
	{ 32,  0 },	// LC8  70 ms
	{ 28,  0 },	// MC8  60 ms
	{ 24,  0 },	// HC8  50 ms
	{  6,  0 },	// RS8  10 ms
	{  8,  0 },	// RS9  15 ms
	{  8,  0 },	// CL8  14 ms
	{ 29,  0 },	// CP8  60 ms tail
	{ 35,  0 },	// CP9  75 ms tail
	{ 12,  0 },	// MA8  22 ms
	{ 31,  0 },	// CB8  70 ms tail
	{  7, 44 },	// HH8  12 ms closed, 110 ms open
	{  8, 55 },	// HH9  15 ms closed, 160 ms open
	{ 79,  0 },	// CY8 350 ms
	{ 87,  0 },	// CR9 450 ms
	{ 89,  0 },	// RD9 500 ms
	{ 84,  0 },	// SRN 400 ms
	{ 93,  0 },	// GND 570 ms
	{ 74,  0 },	// BNG 290 ms
	{120,  0 },	// GNG 1.45 s (10 s to -60 dB, the longest)
};

static const uint16_t parWave[6] = { PAR_OSC_WAVE_DRUM1, PAR_OSC_WAVE_DRUM2, PAR_OSC_WAVE_DRUM3, PAR_OSC_WAVE_SNARE, PAR_WAVE1_CYM, PAR_WAVE1_HH };
static const uint16_t parCoarse[6] = { PAR_COARSE1, PAR_COARSE2, PAR_COARSE3, PAR_COARSE4, PAR_COARSE5, PAR_COARSE6 };
static const uint16_t parFine[6] = { PAR_FINE1, PAR_FINE2, PAR_FINE3, PAR_FINE4, PAR_FINE5, PAR_FINE6 };
static const uint16_t parAttack[6] = { PAR_VELOA1, PAR_VELOA2, PAR_VELOA3, PAR_VELOA4, PAR_VELOA5, PAR_VELOA6 };
static const uint16_t parDecay[6] = { PAR_VELOD1, PAR_VELOD2, PAR_VELOD3, PAR_VELOD4, PAR_VELOD5, PAR_VELOD6_CLOSED };
static const uint16_t parSlope[6] = { PAR_VOL_SLOPE1, PAR_VOL_SLOPE2, PAR_VOL_SLOPE3, PAR_VOL_SLOPE4, PAR_VOL_SLOPE5, PAR_VOL_SLOPE6 };
static const uint16_t parPwm[6] = { PAR_PWM_1, PAR_PWM_2, PAR_PWM_3, PAR_PWM_4, PAR_PWM_5, PAR_PWM_6 };
static const uint16_t parTrans[6] = { PAR_TRANS1_VOL, PAR_TRANS2_VOL, PAR_TRANS3_VOL, PAR_TRANS4_VOL, PAR_TRANS5_VOL, PAR_TRANS6_VOL };
static const uint16_t parFilterType[6] = { PAR_FILTER_TYPE_1, PAR_FILTER_TYPE_2, PAR_FILTER_TYPE_3, PAR_FILTER_TYPE_4, PAR_FILTER_TYPE_5, PAR_FILTER_TYPE_6 };
static const uint16_t parDrive[6] = { PAR_DRIVE1, PAR_DRIVE2, PAR_DRIVE3, PAR_SNARE_DISTORTION, PAR_CYMBAL_DISTORTION, PAR_HAT_DISTORTION };
static const uint16_t parPitchMod[4] = { PAR_MODAMNT1, PAR_MODAMNT2, PAR_MODAMNT3, PAR_MODAMNT4 };
static const uint16_t parFm[3] = { PAR_FMAMNT1, PAR_FMAMNT2, PAR_FMAMNT3 };

//-----------------------------------------------------------------------
uint8_t modelPreset_voiceOfWaveParam(uint16_t parNr)
{
	for(uint8_t v = 0; v < 6; v++)
		if(parWave[v] == parNr)
			return v;
	return MODEL_PRESET_NO_VOICE;
}

//-----------------------------------------------------------------------
void modelPreset_apply(uint8_t voice, uint8_t waveform, ModelPresetSetter set)
{
	if((voice > 5) || !osc_isModel(waveform))
		return;
	const ModelPreset* p = &modelPresets[waveform - MODEL_BD8];

	set(parCoarse[voice], TUNING_ORIGINAL);
	set(parFine[voice], FINE_CENTER);
	set(parAttack[voice], 0);
	set(parDecay[voice], p->decay);
	if(voice == 5)
		set(PAR_VELOD6_OPEN, p->decayOpen ? p->decayOpen : p->decay);
	set(parSlope[voice], SLOPE_GATE);
	set(parPwm[voice], 0);		// tube saturator off
	set(parTrans[voice], 0);
	set(parDrive[voice], 0);
	if(voice != 3)				// the snare voice's filter only works on its noise
		set(parFilterType[voice], FILTER_OFF);
	if(voice < 4)
		set(parPitchMod[voice], 0);
	if(voice < 3)
		set(parFm[voice], 0);
	if(voice == 3)
	{
		set(PAR_MIX1, 0);		// only the instrument, no snare voice noise
		set(PAR_REPEAT4, 0);
	}
	if(voice == 4)
		set(PAR_REPEAT5, 0);
}
