/*
 * host_stubs.c - stand-ins for the parts of the LXR firmware that are not
 * needed to render the drum voices on a PC (sequencer, menu, UART, USB, ADC).
 * Only used by the host render tool, never compiled into the firmware.
 *
 * Part of a custom firmware for the Erica Synths LXR-02 (not an official
 * Erica Synths release). Added 2026.
 * SPDX-License-Identifier: GPL-3.0-only
 */
#include "stm32f7xx_hal.h"
#include "Parameters.h"
#include "Soundengine/Sequencer/sequencer.h"
#include "Soundengine/MIDI/MidiMessages.h"

GPIO_TypeDef host_gpio;
DWT_Type host_dwt;
CoreDebug_Type host_coredebug;

/* deterministic xorshift so renders are reproducible */
static uint32_t host_rng_state = 0x12345678u;
uint32_t GetRngValue(void)
{
	uint32_t x = host_rng_state;
	x ^= x << 13;
	x ^= x >> 17;
	x ^= x << 5;
	host_rng_state = x;
	return x;
}
uint32_t HAL_RNG_GetRandomNumber(RNG_HandleTypeDef *h) { (void)h; return GetRngValue(); }
uint32_t HAL_GetTick(void) { return 0; }
void HAL_Delay(uint32_t ms) { (void)ms; }
void HAL_GPIO_WritePin(GPIO_TypeDef *p, uint16_t pin, GPIO_PinState s) { (void)p; (void)pin; (void)s; }
GPIO_PinState HAL_GPIO_ReadPin(GPIO_TypeDef *p, uint16_t pin) { (void)p; (void)pin; return GPIO_PIN_RESET; }

uint8_t activeParameters[NUM_PARAMS];
float host_faderVolume[7] = {1, 1, 1, 1, 1, 1, 1};
float adc_getFaderVolume(uint8_t idx) { return idx < 7 ? host_faderVolume[idx] : 1.f; }

uint8_t frontParser_activeTrack = 0;
uint8_t menu_activePage = 0;
uint8_t menu_getActiveVoice(void) { return 0; }
uint8_t menu_getSubPage(void) { return 0; }
uint8_t menu_getViewedPattern(void) { return 0; }
void menu_repaint(void) {}
void menu_switchSubPage(uint8_t subPageNr, uint8_t toggle) { (void)subPageNr; (void)toggle; }

uint8_t mixer_audioRouting[6];
float mixer_decimation_rate[7] = {1, 1, 1, 1, 1, 1, 1};

void seq_addNote(uint8_t trackNr, uint8_t vel, uint8_t note) { (void)trackNr; (void)vel; (void)note; }
uint16_t seq_getBpm(void) { return 120; }
uint8_t seq_getExtSync(void) { return 0; }
uint8_t seq_isRunning(void) { return 0; }
uint8_t seq_isTrackMuted(uint8_t trackNr) { (void)trackNr; return 0; }
void seq_midiNoteOff(uint8_t chan) { (void)chan; }
PatternSet seq_patternSet;
void seq_recordAutomation(uint8_t voice, uint8_t dest, uint8_t value) { (void)voice; (void)dest; (void)value; }
void seq_sendMidiNoteOn(const uint8_t channel, const uint8_t note, const uint8_t veloc) { (void)channel; (void)note; (void)veloc; }
void seq_setMute(uint8_t trackNr, uint8_t isMuted) { (void)trackNr; (void)isMuted; }
void seq_setNextPattern(const uint8_t patNr) { (void)patNr; }
void seq_setRunning(uint8_t isRunning) { (void)isRunning; }
void seq_sync(void) {}
void sync_midiResetPosition(void) {}
void sync_midiStartStop(uint8_t isStart) { (void)isStart; }
volatile uint32_t systick_ticks = 0;
void uart_sendFrontpanelByte(uint8_t data) { (void)data; }
void uart_sendMidi(MidiMsg msg) { (void)msg; }
void usb_sendMidi(MidiMsg msg) { (void)msg; }
