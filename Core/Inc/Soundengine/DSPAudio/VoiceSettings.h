/*
 * VoiceSettings.h
 *
 * The settings of each voice in the kit extension (see Parameters.h): the
 * scale of the voice's oscillator and its voice FX. They belong to the kit
 * but are not among the parameters of the official firmware's kit files, so
 * the preset manager saves them behind those.
 *
 * Part of a custom firmware for the Erica Synths LXR-02 (not an official
 * Erica Synths release). Added 2026.
 * SPDX-License-Identifier: GPL-3.0-only
 *
 * Licensed under the GNU General Public License version 3, with the
 * additional permission and terms in NOTICE.md. See LICENSE and NOTICE.md in
 * the root of the repository.
 */

#ifndef VOICESETTINGS_H_
#define VOICESETTINGS_H_

#include <stdint.h>
#include "Parameters.h"

/* scales of the voice's oscillator (PAR_SCALE1..6) */
enum VoiceScales
{
	SCALE_12_TONES,		/* semitones, as in the official firmware */
	SCALE_SLENDRO,
	SCALE_PELOG,
	NUM_SCALES
};

/* voice FX (PAR_VFX_TYPE1..6) */
enum VoiceFxTypes
{
	VFX_OFF,
	VFX_COMPRESSOR,
	VFX_TUBE,
	VFX_FOLD,
	VFX_CLIP,
	VFX_RING,
	NUM_VFX
};

/** applies one setting (PAR_BEGINNING_OF_KIT_EXT <= paramNr < NUM_PARAMS) to the sound engine */
void voiceSettings_set(uint16_t paramNr, uint8_t value);

/** the settings of a new kit, or of a kit saved without them */
void voiceSettings_defaults(uint8_t* ext);

/** keeps loaded settings in their ranges */
void voiceSettings_validate(uint8_t* ext);

#endif /* VOICESETTINGS_H_ */
