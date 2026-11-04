/*
 * plock_seq_access.c - the real sequencer.c plus two helpers for the
 * parameter lock test (its step counters are static). PC only.
 *
 * Part of a custom firmware for the Erica Synths LXR-02 (not an official
 * Erica Synths release). Added 2026.
 * SPDX-License-Identifier: GPL-3.0-only
 */
#include "Soundengine/Sequencer/sequencer.c"

void test_setStepIndex(uint8_t track, int8_t step)
{
	seq_stepIndex[track] = step;
}

uint8_t test_autoNodeDest(uint8_t track, uint8_t slot)
{
	return (uint8_t)seq_automationNodes[track][slot].destination;
}
