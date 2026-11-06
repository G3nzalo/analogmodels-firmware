/*
 * AnalogModelState.h
 *
 * Per-oscillator state of the instrument models (see AnalogModels.h).
 * Kept in its own header so Oscillator.h can embed it in OscInfo without
 * a circular include.
 *
 * Part of a custom firmware for the Erica Synths LXR-02 (not an official
 * Erica Synths release). Added 2026.
 * SPDX-License-Identifier: GPL-3.0-only
 *
 * Licensed under the GNU General Public License version 3, with the
 * additional permission and terms in NOTICE.md. See LICENSE and NOTICE.md in
 * the root of the repository.
 */

#ifndef ANALOGMODELSTATE_H_
#define ANALOGMODELSTATE_H_

#include <stdint.h>

typedef struct ModelStateStruct
{
	float		flt[10];	// filter states
	float		env[10];	// envelopes of the parts or modes of the instrument
	float		ph[8];		// phases [0:1) of the sine oscillators and modes
	float		click;		// trigger pulse low pass
	float		velo;		// velocity of the last hit [0:1]
	float		drift;		// small random pitch offset of the last hit
	float		lastOut;	// last output sample before the saturator
	float		declick;	// remaining part of the previous hit
	float		satX, satY;	// tube saturator: DC blocker
	float		satLow;		// tube saturator: low shelf
	float		satLp;		// tube saturator: high cut
	uint32_t	sq[6];		// square oscillator phases (808 metal, cowbell)
	uint32_t	rng;		// noise generator state
	uint32_t	count;		// samples since the last hit
	uint32_t	nextBurst;	// clap: sample count of the next burst
	uint8_t		burst;		// clap: number of bursts started
	uint8_t		seeded;		// rng and square phases initialised
} ModelState;

#endif /* ANALOGMODELSTATE_H_ */
