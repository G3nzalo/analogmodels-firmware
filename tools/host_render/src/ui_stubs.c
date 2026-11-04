/*
 * ui_stubs.c - empty stand-ins for the hardware, display and SD card
 * functions the user interface code calls, for the parameter lock test
 * (PC only, generated from the linker's list of missing symbols).
 *
 * Part of a custom firmware for the Erica Synths LXR-02 (not an official
 * Erica Synths release). Added 2026.
 * SPDX-License-Identifier: GPL-3.0-only
 */
#include <stdint.h>
int GetRngValue() { return 0; }
int HAL_Delay() { return 0; }
int HAL_RCC_GetHCLKFreq() { return 0; }
int HAL_TIM_Base_Start_IT() { return 0; }
int HAL_TIM_Base_Stop() { return 0; }
int HAL_TIM_Base_Stop_IT() { return 0; }
int adc_getFaderVolume() { return 0; }
unsigned char din_inputData[65536] __attribute__((aligned(64)));
unsigned char dout_outputData[65536] __attribute__((aligned(64)));
unsigned char host_dwt[65536] __attribute__((aligned(64)));
unsigned char host_gpio[65536] __attribute__((aligned(64)));
unsigned char htim2[65536] __attribute__((aligned(64)));
int lcd_clear() { return 0; }
int lcd_data_nonBlock() { return 0; }
int lcd_home() { return 0; }
unsigned char lcd_process_display_interrupt[65536] __attribute__((aligned(64)));
int lcd_setcursor() { return 0; }
int lcd_setcursor_nonBlock() { return 0; }
int lcd_string() { return 0; }
int lcd_turnOn_nonblock() { return 0; }
unsigned char midi_process_block[65536] __attribute__((aligned(64)));
unsigned char mixer_audioRouting[65536] __attribute__((aligned(64)));
unsigned char mixer_decimation_rate[65536] __attribute__((aligned(64)));
int preset_createProject() { return 0; }
/* the real one interpolates towards the morph kit; the test runs with morph = 0 */
extern unsigned char activeParameters[];
unsigned char preset_getMorphValue(unsigned short index, unsigned char morph) { (void)morph; return activeParameters[index]; }
int preset_init() { return 0; }
int preset_isNameEmpty() { return 0; }
int preset_loadDrumsetRam() { return 0; }
int preset_loadGlobals() { return 0; }
int preset_loadPatternRAM() { return 0; }
int preset_loadProject() { return 0; }
int preset_morph() { return 0; }
int preset_saveDrumsetRam() { return 0; }
int preset_saveDrumsetSD() { return 0; }
int preset_saveGlobals() { return 0; }
int preset_savePatternRAM() { return 0; }
int preset_savePatternSD() { return 0; }
int preset_saveProject() { return 0; }
int preset_touchTimer() { return 0; }
unsigned char savefileNames[65536] __attribute__((aligned(64)));
unsigned char screensaver_Active[65536] __attribute__((aligned(64)));
int screensaver_touch() { return 0; }
unsigned char sync_delayedStepTimer[65536] __attribute__((aligned(64)));
int sync_midiResetPosition() { return 0; }
int sync_midiStartStop() { return 0; }
int sync_tick() { return 0; }
unsigned char systick_ticks[65536] __attribute__((aligned(64)));
unsigned char trigger_clockReceived[65536] __attribute__((aligned(64)));
int trigger_clockTick() { return 0; }
unsigned char trigger_dividerClockOut1[65536] __attribute__((aligned(64)));
unsigned char trigger_prescalerClockInput[65536] __attribute__((aligned(64)));
int trigger_reset() { return 0; }
int uart_clearFrontFifo() { return 0; }
int uart_sendFrontpanelByte() { return 0; }
int uart_sendFrontpanelSysExByte() { return 0; }
int uart_sendMidi() { return 0; }
int usb_sendMidi() { return 0; }
