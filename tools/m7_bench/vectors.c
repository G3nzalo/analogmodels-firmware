/*
 * vectors.c - minimal Cortex-M7 start-up for the QEMU mps2-an500 board
 * (CPU cost estimate only, not part of the firmware)
 *
 * Part of a custom firmware for the Erica Synths LXR-02 (not an official
 * Erica Synths release). Added 2026.
 * SPDX-License-Identifier: GPL-3.0-only
 */
#include <stdint.h>

extern uint32_t _stack_top;
extern void _start(void);	/* newlib (rdimon) crt0: clears .bss, sets up semihosting, calls main */

volatile uint32_t bench_sysTickWraps = 0;

void Reset_Handler(void)
{
	/* enable the FPU (CP10/CP11 full access) before any float instruction */
	*(volatile uint32_t*)0xE000ED88 |= (0xFu << 20);
	__asm volatile ("dsb\n isb");
	_start();
	for (;;);
}

void SysTick_Handler(void) { bench_sysTickWraps++; }
static void Default_Handler(void) { for (;;); }

__attribute__((section(".vectors"), used))
const void* vectors[16] = {
	&_stack_top, Reset_Handler, Default_Handler, Default_Handler,
	Default_Handler, Default_Handler, Default_Handler, 0, 0, 0, 0,
	Default_Handler, Default_Handler, 0, Default_Handler, SysTick_Handler,
};
