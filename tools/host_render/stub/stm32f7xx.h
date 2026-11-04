/* Host (x86) stand-in for the STM32 HAL/CMSIS headers, only for rendering the
 * LXR DSP code to WAV files on a PC. Not part of the firmware.
 * SPDX-License-Identifier: GPL-3.0-only */
#ifndef HOST_STM32F7XX_HAL_H
#define HOST_STM32F7XX_HAL_H
#include <stdint.h>
#include <stddef.h>
#include <string.h>
#include <math.h>

#define __IO volatile
#define __I volatile const
#define __O volatile
#define __STATIC_INLINE static inline
#define __INLINE inline
#define __inline inline
#define __ASM __asm__
#ifndef UNUSED
#define UNUSED(x) ((void)(x))
#endif

typedef enum { HAL_OK = 0, HAL_ERROR, HAL_BUSY, HAL_TIMEOUT } HAL_StatusTypeDef;
typedef enum { RESET = 0, SET = !RESET } FlagStatus, ITStatus;
typedef enum { DISABLE = 0, ENABLE = !DISABLE } FunctionalState;
typedef enum { GPIO_PIN_RESET = 0, GPIO_PIN_SET } GPIO_PinState;

typedef struct { volatile uint32_t IDR, ODR, BSRR; } GPIO_TypeDef;
typedef struct { int dummy; } I2S_HandleTypeDef;
typedef struct { int dummy; } UART_HandleTypeDef;
typedef struct { int dummy; } TIM_HandleTypeDef;
typedef struct { int dummy; } ADC_HandleTypeDef;
typedef struct { int dummy; } RNG_HandleTypeDef;
typedef struct { int dummy; } SD_HandleTypeDef;
typedef struct { int dummy; } DMA_HandleTypeDef;
typedef struct { int dummy; } SPI_HandleTypeDef;
typedef struct { int dummy; } PCD_HandleTypeDef;

#define I2S_AUDIOFREQ_44K 44100U
#define GPIO_PIN_0 0x0001U
#define GPIO_PIN_1 0x0002U
#define GPIO_PIN_2 0x0004U
#define GPIO_PIN_3 0x0008U
#define GPIO_PIN_4 0x0010U
#define GPIO_PIN_5 0x0020U
#define GPIO_PIN_6 0x0040U
#define GPIO_PIN_7 0x0080U
#define GPIO_PIN_8 0x0100U
#define GPIO_PIN_9 0x0200U
#define GPIO_PIN_10 0x0400U
#define GPIO_PIN_11 0x0800U
#define GPIO_PIN_12 0x1000U
#define GPIO_PIN_13 0x2000U
#define GPIO_PIN_14 0x4000U
#define GPIO_PIN_15 0x8000U
extern GPIO_TypeDef host_gpio;
#define GPIOA (&host_gpio)
#define GPIOB (&host_gpio)
#define GPIOC (&host_gpio)
#define GPIOD (&host_gpio)
#define GPIOE (&host_gpio)

typedef union { struct { uint32_t _r:27; uint32_t Q:1; uint32_t V:1; uint32_t C:1; uint32_t Z:1; uint32_t N:1; } b; uint32_t w; } APSR_Type;
static inline uint32_t __get_APSR(void) { return 0; }

/* ARM DSP intrinsics (CMSIS) */
static inline int32_t __SSAT_impl(int32_t x, int bits) { int32_t max = (1 << (bits - 1)) - 1, min = -(1 << (bits - 1)); return x > max ? max : (x < min ? min : x); }
#define __SSAT(x, b) __SSAT_impl((int32_t)(x), (b))
static inline uint32_t __USAT_impl(int32_t x, int bits) { int32_t max = (1 << bits) - 1; return x > max ? (uint32_t)max : (x < 0 ? 0u : (uint32_t)x); }
#define __USAT(x, b) __USAT_impl((int32_t)(x), (b))
static inline int16_t host_sat16(int32_t v) { return (int16_t)(v > 32767 ? 32767 : (v < -32768 ? -32768 : v)); }
static inline uint32_t __QADD16(uint32_t a, uint32_t b) { int16_t al = (int16_t)(a & 0xffff), ah = (int16_t)(a >> 16), bl = (int16_t)(b & 0xffff), bh = (int16_t)(b >> 16); return ((uint32_t)(uint16_t)host_sat16(ah + bh) << 16) | (uint16_t)host_sat16(al + bl); }
static inline uint32_t __QSUB16(uint32_t a, uint32_t b) { int16_t al = (int16_t)(a & 0xffff), ah = (int16_t)(a >> 16), bl = (int16_t)(b & 0xffff), bh = (int16_t)(b >> 16); return ((uint32_t)(uint16_t)host_sat16(ah - bh) << 16) | (uint16_t)host_sat16(al - bl); }
static inline uint32_t __SADD16(uint32_t a, uint32_t b) { int16_t al = (int16_t)(a & 0xffff), ah = (int16_t)(a >> 16), bl = (int16_t)(b & 0xffff), bh = (int16_t)(b >> 16); return ((uint32_t)(uint16_t)(int16_t)(ah + bh) << 16) | (uint16_t)(int16_t)(al + bl); }
static inline uint32_t __PKHBT_impl(uint32_t a, uint32_t b, int s) { return (a & 0xffffu) | ((b << s) & 0xffff0000u); }
#define __PKHBT(a, b, s) __PKHBT_impl((uint32_t)(a), (uint32_t)(b), (s))
static inline uint64_t __SMLALD(uint32_t a, uint32_t b, uint64_t acc) { int16_t al = (int16_t)(a & 0xffff), ah = (int16_t)(a >> 16), bl = (int16_t)(b & 0xffff), bh = (int16_t)(b >> 16); return (uint64_t)((int64_t)acc + (int64_t)al * bl + (int64_t)ah * bh); }
static inline uint8_t __CLZ(uint32_t v) { return v ? (uint8_t)__builtin_clz(v) : 32; }

uint32_t HAL_GetTick(void);
uint32_t HAL_RNG_GetRandomNumber(RNG_HandleTypeDef *h);
void HAL_Delay(uint32_t ms);
void HAL_GPIO_WritePin(GPIO_TypeDef *p, uint16_t pin, GPIO_PinState s);
GPIO_PinState HAL_GPIO_ReadPin(GPIO_TypeDef *p, uint16_t pin);

typedef struct { volatile uint32_t CTRL, CYCCNT; } DWT_Type;
extern DWT_Type host_dwt;
#define DWT (&host_dwt)
typedef struct { volatile uint32_t DEMCR; } CoreDebug_Type;
extern CoreDebug_Type host_coredebug;
#define CoreDebug (&host_coredebug)
#define CoreDebug_DEMCR_TRCENA_Msk (1u<<24)
#define DWT_CTRL_CYCCNTENA_Msk 1u
#define SystemCoreClock 216000000u
#define __fp16 _Float16
#endif
