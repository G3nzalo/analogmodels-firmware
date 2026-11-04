/*
 * patfile_stubs.c - empty stand-ins for the hardware, display and timer
 * functions the user interface code calls, for the pattern file test (PC only).
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
int HAL_GetTick() { return 0; }
int soundengine_process() { return 0; }
int uart_checkAndParse() { return 0; }
