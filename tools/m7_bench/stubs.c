/*
 * stubs.c - stand-ins for the parts of the firmware the voice and mixer
 * code reference (CPU cost estimate only, not part of the firmware)
 *
 * Part of a custom firmware for the Erica Synths LXR-02 (not an official
 * Erica Synths release). Added 2026.
 * SPDX-License-Identifier: GPL-3.0-only
 */
#include <stdint.h>
#include "Parameters.h"
static uint32_t rng = 0x12345678u;
uint32_t GetRngValue(void) { uint32_t x = rng; x ^= x << 13; x ^= x >> 17; x ^= x << 5; rng = x; return x; }
uint8_t activeParameters[NUM_PARAMS];
float adc_getFaderVolume(uint8_t idx) { (void)idx; return 1.f; }
uint8_t menu_activePage = 0;
uint8_t menu_getSubPage(void) { return 0; }
void menu_switchSubPage(uint8_t subPageNr, uint8_t toggle) { (void)subPageNr; (void)toggle; }
uint16_t seq_getBpm(void) { return 120; }
void seq_sendMidiNoteOn(const uint8_t channel, const uint8_t note, const uint8_t veloc) { (void)channel; (void)note; (void)veloc; }
void seq_setMute(uint8_t trackNr, uint8_t isMuted) { (void)trackNr; (void)isMuted; }
void uart_sendFrontpanelByte(uint8_t data) { (void)data; }
void _init(void) {}
void _fini(void) {}
