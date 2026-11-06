/*
 * VoiceSettings.c
 *
 * The settings of each voice in the kit extension, see VoiceSettings.h.
 *
 * Part of a custom firmware for the Erica Synths LXR-02 (not an official
 * Erica Synths release). Added 2026.
 * SPDX-License-Identifier: GPL-3.0-only
 *
 * Licensed under the GNU General Public License version 3, with the
 * additional permission and terms in NOTICE.md. See LICENSE and NOTICE.md in
 * the root of the repository.
 */

#include "Soundengine/DSPAudio/VoiceSettings.h"
#include "Soundengine/DSPAudio/DrumVoice.h"
#include "Soundengine/DSPAudio/Snare.h"
#include "Soundengine/DSPAudio/CymbalVoice.h"
#include "Soundengine/DSPAudio/HiHat.h"
#include <math.h>

#define EXT(par)	((par) - PAR_BEGINNING_OF_KIT_EXT)

//-----------------------------------------------------------------------
// Gamelan scales in cents from the first tone; the last value is the octave,
// which gamelans tune a little wide. Every gamelan has its own tuning: these
// are typical Javanese values.
static const int16_t slendroCents[5 + 1] = { 0, 243, 486, 724, 977, 1208 };
static const int16_t pelogCents[7 + 1] = { 0, 120, 258, 539, 675, 785, 943, 1206 };

float voiceSettings_scaleRatio(uint8_t scale, int16_t n)
{
	const int16_t* cents = (scale == SCALE_PELOG) ? pelogCents : slendroCents;
	const int16_t len = (scale == SCALE_PELOG) ? 7 : 5;
	int16_t octave = n / len;
	int16_t degree = n % len;
	if(degree < 0)
	{
		degree += len;
		octave--;
	}
	const float r = exp2f((octave * cents[len] + cents[degree]) * (1.f / 1200.f));
	// the range of the semitones from coarse 0: MIDI notes 0 to 127
	if(r < 0.03125f) return 0.03125f;
	if(r > 47.95f) return 47.95f;
	return r;
}

//-----------------------------------------------------------------------
static OscInfo* voiceSettings_osc(const uint8_t voice)
{
	switch(voice)
	{
	case 0:
	case 1:
	case 2:		return &voiceArray[voice].osc;
	case 3:		return &snareVoice.osc;
	case 4:		return &cymbalVoice.osc;
	default:	return &hatVoice.osc;
	}
}

//-----------------------------------------------------------------------
void voiceSettings_set(uint16_t paramNr, uint8_t value)
{
	if((paramNr >= PAR_SCALE1) && (paramNr <= PAR_SCALE6))
	{
		OscInfo* osc = voiceSettings_osc((uint8_t)(paramNr - PAR_SCALE1));
		osc->scale = (value < NUM_SCALES) ? value : SCALE_12_TONES;
		osc_recalcFreq(osc);
	}
}

//-----------------------------------------------------------------------
void voiceSettings_defaults(uint8_t* ext)
{
	for(uint8_t v = 0; v < 6; v++)
	{
		ext[EXT(PAR_SCALE1) + v] = SCALE_12_TONES;
		ext[EXT(PAR_VFX_TYPE1) + v] = VFX_OFF;
		ext[EXT(PAR_VFX_AMT1) + v] = 0;
		ext[EXT(PAR_VFX_PAR1) + v] = 64;
		ext[EXT(PAR_VFX_SEND1) + v] = 0;
	}
}

//-----------------------------------------------------------------------
void voiceSettings_validate(uint8_t* ext)
{
	for(uint8_t i = 0; i < NUM_KIT_EXT; i++)
		if(ext[i] > 127)
			ext[i] = 127;
	for(uint8_t v = 0; v < 6; v++)
	{
		if(ext[EXT(PAR_SCALE1) + v] >= NUM_SCALES)
			ext[EXT(PAR_SCALE1) + v] = SCALE_12_TONES;
		if(ext[EXT(PAR_VFX_TYPE1) + v] >= NUM_VFX)
			ext[EXT(PAR_VFX_TYPE1) + v] = VFX_OFF;
	}
}
