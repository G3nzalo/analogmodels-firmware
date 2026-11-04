/* QEMU mps2-an500: 4 MB SSRAM1 at 0 (code), 4 MB SSRAM2/3 at 0x20000000 (data)
 * SPDX-License-Identifier: GPL-3.0-only */
ENTRY(Reset_Handler)
MEMORY
{
	CODE (rx)  : ORIGIN = 0x00000000, LENGTH = 4M
	DATA (rwx) : ORIGIN = 0x20000000, LENGTH = 4M
}
SECTIONS
{
	.text : {
		KEEP(*(.vectors))
		*(.text*) *(.itcm_text*)
		*(.rodata*)
		KEEP(*(.init)) KEEP(*(.fini))
		. = ALIGN(4);
		__preinit_array_start = .; KEEP(*(.preinit_array)) __preinit_array_end = .;
		__init_array_start = .; KEEP(*(.init_array*)) __init_array_end = .;
		__fini_array_start = .; KEEP(*(.fini_array*)) __fini_array_end = .;
	} > CODE
	.ARM.exidx : { *(.ARM.exidx*) } > CODE
	.data : { *(.data*) *(.dtcm) *(.dtcm.*) . = ALIGN(8); } > DATA
	.bss (NOLOAD) : {
		__bss_start__ = .; __bss_start = .;
		*(.bss*) *(COMMON) *(.dtcmz) *(.dtcmz.*)
		. = ALIGN(8);
		__bss_end__ = .; _bss_end__ = .; __end__ = .; end = .;
	} > DATA
	_stack_top = ORIGIN(DATA) + LENGTH(DATA);
	__stack = _stack_top;
	__heap_limit = _stack_top - 0x10000;
}
