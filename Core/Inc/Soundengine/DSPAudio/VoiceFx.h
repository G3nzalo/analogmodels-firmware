/*
 * VoiceFx.h
 *
 * The voice FX: an effect at the end of each voice, after its filter, drive
 * and sample rate reduction and before its output: a compressor, three
 * distortions (tube, wavefolder, clipper) or a ring modulator. The settings
 * are part of the kit extension (PAR_VFX_TYPE1..6, PAR_VFX_AMT1..6 and
 * PAR_VFX_PAR1..6, see VoiceSettings.h).
 *
 * Part of a custom firmware for the Erica Synths LXR-02 (not an official
 * Erica Synths release). Added 2026.
 * SPDX-License-Identifier: GPL-3.0-only
 *
 * Licensed under the GNU General Public License version 3, with the
 * additional permission and terms in NOTICE.md. See LICENSE and NOTICE.md in
 * the root of the repository.
 */

#ifndef VOICEFX_H_
#define VOICEFX_H_

#include <stdint.h>

/** settings of a voice's FX, from the knob values 0..127 */
void voiceFx_setType(uint8_t voice, uint8_t type);
void voiceFx_setAmount(uint8_t voice, uint8_t value);
void voiceFx_setParam(uint8_t voice, uint8_t value);

/** runs the FX of a voice on one block of its samples */
void voiceFx_process(uint8_t voice, int16_t* buf, uint8_t size);

#endif /* VOICEFX_H_ */
