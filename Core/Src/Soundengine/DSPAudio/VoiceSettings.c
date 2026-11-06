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

#define EXT(par)	((par) - PAR_BEGINNING_OF_KIT_EXT)

//-----------------------------------------------------------------------
void voiceSettings_set(uint16_t paramNr, uint8_t value)
{
	(void)paramNr;
	(void)value;
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
