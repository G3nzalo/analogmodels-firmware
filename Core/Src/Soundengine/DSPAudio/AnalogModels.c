/*
 * AnalogModels.c
 *
 * The instruments of the TR-808 and TR-909 for the LXR-02 voices, modelled
 * after their circuits, and acoustic instruments modelled after their modes
 * of vibration (see AnalogModels.h for the list). Each instrument
 * renders its own pitch sweeps, clicks, noise and envelopes. The time scale of
 * the envelopes follows the voice's amp decay knob: the amp EG of the voice is
 * set up as a gate (see menu/modelPresets.c) and the instrument has faded to
 * -60 dB when the gate closes. The voice's PWM knob is a tube saturator.
 *
 * Reference values: TR-808 bass drum after K. J. Werner et al., "A
 * Physically-Informed, Circuit-Bendable, Digital Model of the Roland TR-808
 * Bass Drum Circuit" (DAFx 2014); the 808 cymbal oscillators and band pass
 * frequencies after Werner, Abel and Smith (ICMC/SMC 2014); the other
 * frequencies from published circuit analyses and tuning charts.
 *
 * All instruments run instead of the oscillator they replace. Nothing here
 * runs from ITCM.
 *
 * Part of a custom firmware for the Erica Synths LXR-02 (not an official
 * Erica Synths release). Added 2026.
 * SPDX-License-Identifier: GPL-3.0-only
 *
 * Licensed under the GNU General Public License version 3, with the
 * additional permission and terms in NOTICE.md. See LICENSE and NOTICE.md in
 * the root of the repository.
 */

#include "Soundengine/DSPAudio/AnalogModels.h"
#include "Soundengine/DSPAudio/random.h"
#include "Soundengine/config.h"
#include <math.h>
#include <string.h>

#define M_FS			REAL_FS
#define M_INV_FS		(1.f / REAL_FS)
#define PHASE_TO_HZ		(REAL_FS / 4294967296.f)
#define HZ_TO_PHASE		(4294967296.f / REAL_FS)
#define PHASE_TO_UNIT	(1.f / 4294967296.f)
#define M_PI_F			3.14159265358979f
#define LN_60DB			6.9078f		// ln(1000): an envelope is at -60 dB after LN_60DB time constants
#define MS				(0.001f * REAL_FS)	// samples per millisecond

//-----------------------------------------------------------------------
// helpers
//-----------------------------------------------------------------------
static inline float clampf(const float x, const float lo, const float hi)
{
	return x < lo ? lo : (x > hi ? hi : x);
}

static inline float maxf(const float a, const float b)
{
	return a > b ? a : b;
}

static inline int16_t toInt16(const float x)
{
	if(x > 32767.f) return 32767;
	if(x < -32767.f) return -32767;
	return (int16_t)x;
}

static inline uint32_t rngNext(ModelState* s)
{
	uint32_t x = s->rng;
	x ^= x << 13;
	x ^= x >> 17;
	x ^= x << 5;
	s->rng = x;
	return x;
}

/** uniform white noise in [-1:1) */
static inline float rngFloat(ModelState* s)
{
	return (int32_t)rngNext(s) * 4.656612873e-10f;
}

/** sin(2 pi q) for a phase 0 <= q < 1, within 0.1 % */
static inline float sinCycle(const float q)
{
	const float x = 2.f * q - 1.f;
	float y = 4.f * x * (1.f - fabsf(x));
	y = 0.225f * (y * fabsf(y) - y) + y;
	return -y;
}

/** triangle for a phase 0 <= q < 1: starts at 0 and rises, like a sine */
static inline float triCycle(const float q)
{
	if(q < 0.25f) return 4.f * q;
	if(q < 0.75f) return 2.f - 4.f * q;
	return 4.f * q - 4.f;
}

/** advance a phase by inc >= 0 and wrap it to 0 <= q < 1. inc can be above
 * 1 when an instrument is tuned far up: it aliases then, but stays a tone. */
static inline float phaseAdd(const float q, const float inc)
{
	const float p = q + inc;
	return p - (float)(int32_t)p;
}

/** level of a partial at the phase increment inc: muted above 0.45 (about
 * 20 kHz), where it would only fold back as an alias */
static inline float partialGain(const float inc)
{
	return inc < 0.45f ? 1.f : 0.f;
}

/** tan(x) for 0 <= x <= 0.7 */
static inline float fastTan(const float x)
{
	const float x2 = x * x;
	return x * (1.f + x2 * (1.f / 3.f + x2 * (2.f / 15.f + x2 * (17.f / 315.f))));
}

/** smooth saturation, tanh(x) within 0.3 % for |x| <= 3, exactly +-1 beyond */
static inline float softSat(float x)
{
	x = clampf(x, -3.f, 3.f);
	const float x2 = x * x;
	return x * (27.f + x2) / (27.f + 9.f * x2);
}

/** output limiter: transparent up to 0.8, then a soft knee that reaches full
 * scale at 1.4 and stays there. Catches the overlap of a retriggered hit and
 * extreme settings without the hard clipping of the 16 bit output. */
static inline float kneeLimit(const float x)
{
	const float ax = fabsf(x);
	if(ax <= 0.8f)
		return x;
	const float y = 0.8f + 0.2f * softSat((ax - 0.8f) * 5.f);
	return x < 0.f ? -y : y;
}

/** per sample factor of an exponential decay with time constant tau [s] */
static inline float decayRate(const float tau)
{
	return expf(-M_INV_FS / maxf(tau, 0.0002f));
}

/** coefficient of a one pole low pass at fc [Hz] */
static inline float onePole(const float fc)
{
	return 1.f - expf(-2.f * M_PI_F * clampf(fc, 5.f, 0.45f * M_FS) * M_INV_FS);
}

/** band limited (polyBLEP) correction for a step at phase 0 */
static inline float blep(float t, const float dt, const float invDt)
{
	if(t < dt)
	{
		t *= invDt;
		return t + t - t * t - 1.f;
	}
	if(t > 1.f - dt)
	{
		t = (t - 1.f) * invDt;
		return t * t + t + t + 1.f;
	}
	return 0.f;
}

/** band limited square from a 32 bit phase */
static inline float blepSquare(const uint32_t phase, const float dt, const float invDt)
{
	const float t = phase * PHASE_TO_UNIT;
	const float t2 = (uint32_t)(phase + 0x80000000u) * PHASE_TO_UNIT;
	return (t < 0.5f ? 1.f : -1.f) + blep(t, dt, invDt) - blep(t2, dt, invDt);
}

static inline uint32_t hzToInc(const float f)
{
	return (uint32_t)(clampf(f, 0.f, 0.45f * M_FS) * HZ_TO_PHASE);
}

/** coefficients of a topology preserving state variable filter, k = 1/Q */
typedef struct { float a1, a2, a3, k; } SvfCoef;

static inline SvfCoef svf_coef(const float fc, const float k)
{
	SvfCoef c;
	const float g = fastTan(M_PI_F * clampf(fc, 20.f, 0.2f * M_FS) * M_INV_FS);
	c.k = k;
	c.a1 = 1.f / (1.f + g * (g + k));
	c.a2 = g * c.a1;
	c.a3 = g * c.a2;
	return c;
}

/** one filter step on the states st[0], st[1]: returns the band pass (peak
 * gain 1/k) and writes the low pass. High pass = in - k * band - low. */
static inline float svf_tick(const SvfCoef* c, float* st, const float in, float* lowOut)
{
	const float v3 = in - st[1];
	const float v1 = c->a1 * st[0] + c->a2 * v3;
	const float v2 = st[1] + c->a2 * st[0] + c->a3 * v3;
	st[0] = 2.f * v1 - st[0];
	st[1] = 2.f * v2 - st[1];
	*lowOut = v2;
	return v1;
}

static inline float svf_high(const SvfCoef* c, float* st, const float in)
{
	float low;
	const float band = svf_tick(c, st, in, &low);
	return in - c->k * band - low;
}

/** band pass normalised to a peak gain of 1 */
static inline float svf_band(const SvfCoef* c, float* st, const float in)
{
	float low;
	return c->k * svf_tick(c, st, in, &low);
}

/** what every instrument needs to know about the current block */
typedef struct
{
	float	tf;		// tuning: 1 at the original tuning (coarse and fine at 0)
	float	T;		// length of the voice's amp gate [s] (the decay knob)
	float	tau;	// main time constant [s], -60 dB when the gate closes
	float	velo;	// velocity [0:1]
	uint32_t t0;	// samples since the hit at the start of the block
} ModelCtx;

//-----------------------------------------------------------------------
// the 808 metal: six square oscillators. 205.3, 304.4, 369.6 and 522.7 Hz are
// fixed, the cowbell pair is trimmed to about 540 and 800 Hz.
//-----------------------------------------------------------------------
static const float metal808[6] = { 205.3f, 304.4f, 369.6f, 522.7f, 540.f, 800.f };

typedef struct { uint32_t inc[6]; float dt[6], idt[6]; } SquareBank;

static inline void squares_set(SquareBank* b, const float* f, const uint8_t num, const float tf)
{
	for(uint8_t k = 0; k < num; k++)
	{
		b->inc[k] = hzToInc(f[k] * tf);
		b->dt[k] = b->inc[k] * PHASE_TO_UNIT;
		b->idt[k] = b->dt[k] > 0.f ? 1.f / b->dt[k] : 0.f;
	}
}

static inline float squares_tick(ModelState* s, const SquareBank* b, const uint8_t first, const uint8_t num)
{
	float sum = 0.f;
	for(uint8_t k = first; k < first + num; k++)
	{
		sum += blepSquare(s->sq[k], b->dt[k], b->idt[k]);
		s->sq[k] += b->inc[k];
	}
	return sum;
}

//-----------------------------------------------------------------------
// the 909 cymbals and hats are samples in the original. A dense metal for them:
// six squares ring modulated in pairs (the sign of the XOR of two phases is
// the product of two squares), cheap and very inharmonic.
//-----------------------------------------------------------------------
static const float cymRatio[6] = { 1.f, 1.3512f, 1.7433f, 2.1849f, 2.8017f, 3.6529f };

static inline void ringMetal_set(uint32_t* inc, const float f)
{
	for(uint8_t k = 0; k < 6; k++)
		inc[k] = hzToInc(f * cymRatio[k]);
}

static inline float ringMetal_tick(ModelState* s, const uint32_t* inc)
{
	const int32_t a = (int32_t)(s->sq[0] ^ s->sq[1]);
	const int32_t b = (int32_t)(s->sq[2] ^ s->sq[3]);
	const int32_t c = (int32_t)(s->sq[4] ^ s->sq[5]);
	for(uint8_t k = 0; k < 6; k++)
		s->sq[k] += inc[k];
	return ((a < 0 ? -1.f : 1.f) + (b < 0 ? -1.f : 1.f) + (c < 0 ? -1.f : 1.f)) * (1.f / 3.f);
}

//-----------------------------------------------------------------------
// BD8: 808 bass drum. A bridged-T resonator at about 52 Hz that runs more
// than an octave higher for the first ~5 ms of the hit (the envelope switch
// in the circuit), a slight "sigh" as it fades and the trigger pulse through
// the tone low pass as the click.
//-----------------------------------------------------------------------
static void model_bd8(ModelState* s, float* out, const uint8_t n, const ModelCtx* c)
{
	const float fBase = 52.f * c->tf;
	const float rA = decayRate(c->tau);
	const float rP = decayRate(0.0009f);
	const float lpC = onePole(1600.f);
	const float clickAmp = 0.22f + 0.2f * c->velo;
	float q = s->ph[0], a = s->env[0], p = s->env[1], cl = s->click;
	uint32_t t = c->t0;
	for(uint8_t i = 0; i < n; i++, t++)
	{
		if(t > (uint32_t)(4.5f * MS))
			p *= rP;
		const float f = fBase * (1.f + 1.25f * p + 0.03f * a);
		q = phaseAdd(q, f * M_INV_FS);
		cl += lpC * (((t < (uint32_t)MS) ? clickAmp : 0.f) - cl);
		out[i] = 0.92f * a * sinCycle(q) + cl;
		a *= rA;
	}
	s->ph[0] = q; s->env[0] = a; s->env[1] = p; s->click = cl;
}

//-----------------------------------------------------------------------
// BD9: 909 bass drum. A triangle VCO shaped towards a sine, a fast pitch
// sweep from about 220 Hz down to 54 Hz, an amp envelope that holds for a
// moment before it decays, and the attack click (a short pulse and a burst
// of noise).
//-----------------------------------------------------------------------
static void model_bd9(ModelState* s, float* out, const uint8_t n, const ModelCtx* c)
{
	const float fBase = 54.f * c->tf;
	const float hold = 10.f * MS;
	const float rA = decayRate(maxf(c->T - 0.01f, 0.005f) * (1.f / LN_60DB));
	const float rP1 = decayRate(0.0035f), rP2 = decayRate(0.028f);
	const float rN = decayRate(0.0012f);
	const float lpC = onePole(6500.f), hpC = onePole(450.f);
	const float clickAmp = 0.35f + 0.35f * c->velo;
	const float norm = 1.f / softSat(1.3f);
	float q = s->ph[0], a = s->env[0], p1 = s->env[1], p2 = s->env[2], nz = s->env[3];
	float lp = s->flt[0], hp = s->flt[1];
	uint32_t t = c->t0;
	for(uint8_t i = 0; i < n; i++, t++)
	{
		const float f = fBase * (1.f + 2.2f * p1 + 0.9f * p2);
		p1 *= rP1;
		p2 *= rP2;
		q = phaseAdd(q, f * M_INV_FS);
		const float tri = triCycle(q);
		const float w = tri * (1.5f - 0.5f * tri * tri);
		if(t >= (uint32_t)hold)
			a *= rA;
		// click: 0.7 ms pulse and a noise burst, band limited
		const float x = ((t < (uint32_t)(0.7f * MS)) ? 0.6f : 0.f) + 0.7f * nz * rngFloat(s);
		nz *= rN;
		lp += lpC * (x - lp);
		hp += hpC * (lp - hp);
		const float o = a * w + clickAmp * (lp - hp);
		out[i] = softSat(1.3f * o) * norm;
	}
	s->ph[0] = q; s->env[0] = a; s->env[1] = p1; s->env[2] = p2; s->env[3] = nz;
	s->flt[0] = lp; s->flt[1] = hp;
}

//-----------------------------------------------------------------------
// SD8: 808 snare. Two bridged-T resonators at 238 and 476 Hz with shorter
// decays than the "snappy": white noise through a high pass.
//-----------------------------------------------------------------------
static void model_sd8(ModelState* s, float* out, const uint8_t n, const ModelCtx* c)
{
	const float i1 = 238.f * c->tf * M_INV_FS, i2 = 476.f * c->tf * M_INV_FS;
	const float r1 = decayRate(0.75f * c->tau), r2 = decayRate(0.45f * c->tau), rS = decayRate(c->tau);
	const SvfCoef hp = svf_coef(1800.f * c->tf, 1.2f);
	const float lpN = onePole(11000.f);
	const float snap = 0.5f * (0.6f + 0.4f * c->velo);
	const float g1 = 0.62f * partialGain(i1), g2 = 0.38f * partialGain(i2);
	float q1 = s->ph[0], q2 = s->ph[1], a1 = s->env[0], a2 = s->env[1], an = s->env[2], lp = s->flt[2];
	for(uint8_t i = 0; i < n; i++)
	{
		q1 = phaseAdd(q1, i1);
		q2 = phaseAdd(q2, i2);
		lp += lpN * (rngFloat(s) - lp);
		const float nz = svf_high(&hp, &s->flt[0], lp);
		out[i] = g1 * a1 * sinCycle(q1) + g2 * a2 * sinCycle(q2) + snap * an * nz;
		a1 *= r1;
		a2 *= r2;
		an *= rS;
	}
	s->ph[0] = q1; s->ph[1] = q2; s->env[0] = a1; s->env[1] = a2; s->env[2] = an; s->flt[2] = lp;
}

//-----------------------------------------------------------------------
// SD9: 909 snare. Two shaped triangle VCOs at about 180 and 330 Hz with a
// short pitch drop, and noise: a long high passed "snappy" part and a short
// burst of the full noise.
//-----------------------------------------------------------------------
static void model_sd9(ModelState* s, float* out, const uint8_t n, const ModelCtx* c)
{
	const float i1 = 180.f * c->tf * M_INV_FS, i2 = 330.f * c->tf * M_INV_FS;
	const float r1 = decayRate(0.55f * c->tau), r2 = decayRate(0.35f * c->tau), rS = decayRate(c->tau);
	const float rB = decayRate(0.010f), rP = decayRate(0.006f);
	const SvfCoef hp = svf_coef(3000.f * c->tf, 1.1f);
	const float lpN = onePole(7000.f);
	const float snap = 0.6f * (0.6f + 0.4f * c->velo);
	float q1 = s->ph[0], q2 = s->ph[1], a1 = s->env[0], a2 = s->env[1], an = s->env[2], ab = s->env[3];
	float p = s->click, lp = s->flt[2];
	for(uint8_t i = 0; i < n; i++)
	{
		const float pm = 1.f + 0.35f * p;
		p *= rP;
		q1 = phaseAdd(q1, i1 * pm);
		q2 = phaseAdd(q2, i2 * pm);
		const float t1 = triCycle(q1), t2 = triCycle(q2);
		const float w1 = t1 * (1.5f - 0.5f * t1 * t1), w2 = t2 * (1.5f - 0.5f * t2 * t2);
		lp += lpN * (rngFloat(s) - lp);
		const float nz = svf_high(&hp, &s->flt[0], lp);
		out[i] = 0.5f * a1 * w1 + 0.32f * a2 * w2 + snap * an * nz + 0.3f * ab * lp;
		a1 *= r1;
		a2 *= r2;
		an *= rS;
		ab *= rB;
	}
	s->ph[0] = q1; s->ph[1] = q2; s->env[0] = a1; s->env[1] = a2; s->env[2] = an; s->env[3] = ab;
	s->click = p; s->flt[2] = lp;
}

//-----------------------------------------------------------------------
// 808 toms and congas: a bridged-T resonator with a slight pitch bend at the
// attack. The toms add a little low passed noise that rings a bit longer.
//-----------------------------------------------------------------------
typedef struct { float f, bend, bendTau, noise; } Tom808;

static const Tom808 tom808[6] =
{
	{  90.f, 0.12f, 0.025f, 0.04f },	// LT8
	{ 135.f, 0.12f, 0.022f, 0.04f },	// MT8
	{ 185.f, 0.12f, 0.020f, 0.04f },	// HT8
	{ 185.f, 0.16f, 0.012f, 0.f },		// LC8
	{ 250.f, 0.16f, 0.011f, 0.f },		// MC8
	{ 400.f, 0.16f, 0.010f, 0.f },		// HC8
};

static void model_tom808(ModelState* s, float* out, const uint8_t n, const ModelCtx* c, const Tom808* d)
{
	const float fBase = d->f * c->tf;
	const float rA = decayRate(c->tau), rB = decayRate(d->bendTau), rN = decayRate(1.3f * c->tau);
	const float lpN = onePole(1200.f), lpC = onePole(2500.f);
	float q = s->ph[0], a = s->env[0], b = s->env[1], an = s->env[2], lp = s->flt[0], cl = s->click;
	uint32_t t = c->t0;
	for(uint8_t i = 0; i < n; i++, t++)
	{
		q = phaseAdd(q, fBase * (1.f + d->bend * b + 0.03f * a) * M_INV_FS);
		lp += lpN * (rngFloat(s) - lp);
		cl += lpC * (((t < (uint32_t)(0.5f * MS)) ? 0.12f : 0.f) - cl);
		out[i] = 0.9f * a * sinCycle(q) + d->noise * 2.5f * an * lp + cl;
		a *= rA;
		b *= rB;
		an *= rN;
	}
	s->ph[0] = q; s->env[0] = a; s->env[1] = b; s->env[2] = an; s->flt[0] = lp; s->click = cl;
}

//-----------------------------------------------------------------------
// 909 toms: a shaped triangle VCO with a clear pitch dive and a short burst
// of low passed noise.
//-----------------------------------------------------------------------
static const float tom909[3] = { 110.f, 160.f, 220.f };	// LT9, MT9, HT9 end pitch [Hz]

static void model_tom909(ModelState* s, float* out, const uint8_t n, const ModelCtx* c, const float fEnd)
{
	const float fBase = fEnd * c->tf;
	const float rA = decayRate(c->tau), rP = decayRate(0.04f), rN = decayRate(0.03f);
	const float lpN = onePole(3500.f);
	float q = s->ph[0], a = s->env[0], p = s->env[1], an = s->env[2], lp = s->flt[0];
	for(uint8_t i = 0; i < n; i++)
	{
		q = phaseAdd(q, fBase * (1.f + 0.75f * p) * M_INV_FS);
		const float tri = triCycle(q);
		const float w = 0.8f * tri * (1.5f - 0.5f * tri * tri) + 0.2f * tri;
		lp += lpN * (rngFloat(s) - lp);
		out[i] = 0.88f * a * w + 0.3f * an * lp;
		a *= rA;
		p *= rP;
		an *= rN;
	}
	s->ph[0] = q; s->env[0] = a; s->env[1] = p; s->env[2] = an; s->flt[0] = lp;
}

//-----------------------------------------------------------------------
// RS8: 808 rim shot. Two short resonators at 455 and 1667 Hz, high passed
// and driven hard. RS9: 909 rim shot, three short resonators.
// CL8: 808 claves, one resonator at 2.5 kHz.
//-----------------------------------------------------------------------
typedef struct { float f[3], tau[3], amp[3], hp, drive; } RimDef;

static const RimDef rim808 = { { 455.f, 1667.f, 0.f }, { 1.f, 0.7f, 1.f }, { 0.55f, 0.75f, 0.f }, 420.f, 2.f };
static const RimDef rim909 = { { 480.f, 1170.f, 1690.f }, { 1.f, 0.75f, 0.55f }, { 0.55f, 0.45f, 0.35f }, 300.f, 2.5f };
static const RimDef clave808 = { { 2500.f, 0.f, 0.f }, { 1.f, 1.f, 1.f }, { 1.f, 0.f, 0.f }, 60.f, 1.2f };

static void model_rim(ModelState* s, float* out, const uint8_t n, const ModelCtx* c, const RimDef* d)
{
	float inc[3], r[3], amp[3];
	for(uint8_t k = 0; k < 3; k++)
	{
		inc[k] = d->f[k] * c->tf * M_INV_FS;
		r[k] = decayRate(d->tau[k] * c->tau);
		amp[k] = d->amp[k] * partialGain(inc[k]);
	}
	const SvfCoef hp = svf_coef(d->hp * c->tf, 1.f);
	const float norm = 1.f / softSat(d->drive);
	for(uint8_t i = 0; i < n; i++)
	{
		float x = 0.f;
		for(uint8_t k = 0; k < 3; k++)
		{
			s->ph[k] = phaseAdd(s->ph[k], inc[k]);
			x += amp[k] * s->env[k] * sinCycle(s->ph[k]);
			s->env[k] *= r[k];
		}
		out[i] = softSat(d->drive * svf_high(&hp, &s->flt[0], x)) * norm;
	}
}

//-----------------------------------------------------------------------
// CP8/CP9: hand claps. Band passed noise with a few sawtooth shaped bursts
// about 10 ms apart, then the "reverb" tail.
//-----------------------------------------------------------------------
typedef struct { float f, k; uint8_t bursts; float spacing, burstTau, tail, sizzle; } ClapDef;

static const ClapDef clap808 = { 1000.f, 0.62f, 3, 0.010f, 0.0035f, 0.55f, 0.f };
static const ClapDef clap909 = { 1140.f, 0.51f, 4, 0.011f, 0.003f, 0.6f, 0.15f };

static void model_clap(ModelState* s, float* out, const uint8_t n, const ModelCtx* c, const ClapDef* d)
{
	const float span = (d->bursts - 1) * d->spacing;
	const float rB = decayRate(d->burstTau);
	const float rT = decayRate(maxf(c->T - span, 0.01f) * (1.f / LN_60DB));
	const SvfCoef bp = svf_coef(d->f * c->tf, d->k);
	const SvfCoef hp = svf_coef(6000.f * c->tf, 1.2f);
	float be = s->env[0], te = s->env[1];
	uint32_t t = c->t0;
	for(uint8_t i = 0; i < n; i++, t++)
	{
		if((s->burst < d->bursts) && (t >= s->nextBurst))
		{
			be = 1.f;
			s->burst++;
			if(s->burst == d->bursts)
				te = 1.f;	// the tail starts with the last burst
			s->nextBurst = t + (uint32_t)(d->spacing * M_FS * (1.f + 0.06f * rngFloat(s)));
		}
		const float nz = rngFloat(s);
		const float x = svf_band(&bp, &s->flt[0], nz);
		const float hiss = (d->sizzle > 0.f) ? d->sizzle * te * svf_high(&hp, &s->flt[2], nz) : 0.f;
		out[i] = 1.6f * (be + d->tail * te) * x + hiss;
		be *= rB;
		te *= rT;
	}
	s->env[0] = be; s->env[1] = te;
}

//-----------------------------------------------------------------------
// MA8: 808 maracas. High passed noise with a slow attack.
//-----------------------------------------------------------------------
static void model_ma8(ModelState* s, float* out, const uint8_t n, const ModelCtx* c)
{
	const float att = 6.f * MS;
	const float rA = decayRate(maxf(c->T - 0.006f, 0.003f) * (1.f / LN_60DB));
	const SvfCoef hp = svf_coef(5000.f * c->tf, 1.f);
	float a = s->env[0];
	uint32_t t = c->t0;
	for(uint8_t i = 0; i < n; i++, t++)
	{
		float e;
		if(t < (uint32_t)att)
			e = t * (1.f / att);
		else
		{
			e = a;
			a *= rA;
		}
		out[i] = 0.85f * e * svf_high(&hp, &s->flt[0], rngFloat(s));
	}
	s->env[0] = a;
}

//-----------------------------------------------------------------------
// CB8: 808 cowbell. The 540 and 800 Hz squares through a band pass, with a
// sharp drop right after the impact and a longer tail.
//-----------------------------------------------------------------------
static void model_cb8(ModelState* s, float* out, const uint8_t n, const ModelCtx* c)
{
	SquareBank b;
	squares_set(&b, metal808, 6, c->tf);
	const SvfCoef bp = svf_coef(2640.f * c->tf, 1.1f);
	const float rI = decayRate(0.003f), rT = decayRate(c->tau);
	float ai = s->env[0], at = s->env[1];
	for(uint8_t i = 0; i < n; i++)
	{
		const float sq = 0.5f * squares_tick(s, &b, 4, 2);
		const float x = svf_band(&bp, &s->flt[0], sq) + 0.15f * sq;
		out[i] = 1.1f * (0.55f * ai + 0.45f * at) * x;
		ai *= rI;
		at *= rT;
	}
	s->env[0] = ai; s->env[1] = at;
}

//-----------------------------------------------------------------------
// HH8: 808 hi-hat, closed or open by the decay knob. The six squares through
// the upper band pass and a high pass.
//-----------------------------------------------------------------------
static void model_hh8(ModelState* s, float* out, const uint8_t n, const ModelCtx* c)
{
	SquareBank b;
	squares_set(&b, metal808, 6, c->tf);
	const SvfCoef bp = svf_coef(7100.f * c->tf, 0.9f);
	const SvfCoef hp = svf_coef(6000.f * c->tf, 1.3f);
	const float rA = decayRate(c->tau);
	const float lvl = 1.9f + 0.6f * c->velo;
	float a = s->env[0];
	for(uint8_t i = 0; i < n; i++)
	{
		const float m = squares_tick(s, &b, 0, 6) * (1.f / 6.f);
		const float x = svf_high(&hp, &s->flt[2], svf_band(&bp, &s->flt[0], m));
		out[i] = softSat(lvl * a * x);
		a *= rA;
	}
	s->env[0] = a;
}

//-----------------------------------------------------------------------
// HH9: 909 hi-hat. The original plays a 6 bit sample of a real hi-hat; a
// dense metal and noise, high passed and reduced to 6 bit, then enveloped.
//-----------------------------------------------------------------------
static void model_hh9(ModelState* s, float* out, const uint8_t n, const ModelCtx* c)
{
	uint32_t inc[6];
	ringMetal_set(inc, 380.f * c->tf);
	const SvfCoef hp = svf_coef(7500.f * c->tf, 1.2f);
	const float rA = decayRate(c->tau);
	const float lpO = onePole(13000.f);
	float a = s->env[0], lp = s->flt[2];
	for(uint8_t i = 0; i < n; i++)
	{
		const float x = 0.6f * ringMetal_tick(s, inc) + 0.4f * rngFloat(s);
		float h = clampf(1.6f * svf_high(&hp, &s->flt[0], x), -1.f, 1.f);
		h = floorf(h * 31.5f + 0.5f) * (1.f / 31.5f);	// 6 bit, like the sample ROM
		lp += lpO * (h - lp);
		out[i] = 0.8f * (0.7f + 0.3f * c->velo) * a * lp;
		a *= rA;
	}
	s->env[0] = a; s->flt[2] = lp;
}

//-----------------------------------------------------------------------
// CY8: 808 cymbal. The six squares split into three bands with their own
// decays: the low band (3.44 kHz) rings longest, the high band shortest.
//-----------------------------------------------------------------------
static void model_cy8(ModelState* s, float* out, const uint8_t n, const ModelCtx* c)
{
	SquareBank b;
	squares_set(&b, metal808, 6, c->tf);
	const SvfCoef bpL = svf_coef(3440.f * c->tf, 1.f), hpL = svf_coef(2000.f * c->tf, 1.4f);
	const SvfCoef bpM = svf_coef(7100.f * c->tf, 1.f), hpM = svf_coef(5000.f * c->tf, 1.4f);
	const SvfCoef hpH = svf_coef(10500.f * c->tf, 0.8f);
	const float rL = decayRate(c->tau), rM = decayRate(0.4f * c->tau), rH = decayRate(0.18f * c->tau);
	float aL = s->env[0], aM = s->env[1], aH = s->env[2];
	for(uint8_t i = 0; i < n; i++)
	{
		const float m = squares_tick(s, &b, 0, 6) * (1.f / 6.f);
		const float lo = svf_high(&hpL, &s->flt[6], svf_band(&bpL, &s->flt[0], m));
		const float mid = svf_high(&hpM, &s->flt[8], svf_band(&bpM, &s->flt[2], m));
		const float hi = svf_high(&hpH, &s->flt[4], m);
		out[i] = softSat(2.2f * (0.55f * aL * lo + 0.7f * aM * mid + 0.5f * aH * hi));
		aL *= rL;
		aM *= rM;
		aH *= rH;
	}
	s->env[0] = aL; s->env[1] = aM; s->env[2] = aH;
}

//-----------------------------------------------------------------------
// CR9: crash. Metal and noise through a high pass, with a bright strike and
// a low pass that closes as the crash fades.
//-----------------------------------------------------------------------
static void model_cr9(ModelState* s, float* out, const uint8_t n, const ModelCtx* c)
{
	uint32_t inc[6];
	ringMetal_set(inc, 520.f * c->tf);
	const SvfCoef hp = svf_coef(3000.f * c->tf, 1.2f);
	const float rA = decayRate(c->tau), rS = decayRate(0.03f);
	const float metalMix = 0.42f;
	float a = s->env[0], strike = s->env[1], lp = s->flt[2];
	// brighter on hard hits, darker while it fades (updated once per block)
	const float bright = clampf(c->velo * (0.35f + 0.65f * a), 0.f, 1.f);
	const float lpA = onePole(2500.f + 14000.f * bright);
	for(uint8_t i = 0; i < n; i++)
	{
		const float nz = rngFloat(s);
		const float x = metalMix * ringMetal_tick(s, inc) + (1.f - metalMix + 0.8f * strike) * nz;
		lp += lpA * (svf_high(&hp, &s->flt[0], x) - lp);
		out[i] = softSat(0.75f * lp) * a;
		a *= rA;
		strike *= rS;
	}
	s->env[0] = a; s->env[1] = strike; s->flt[2] = lp;
}

//-----------------------------------------------------------------------
// RD9: ride. A short stick ping, four inharmonic bell partials and a metal
// wash. The ring times scale with the decay knob.
//-----------------------------------------------------------------------
static const float rideRatio[4] = { 1.f, 1.594f, 2.235f, 2.941f };
static const float rideTau[4] = { 3.2f, 2.2f, 1.6f, 1.1f };	// ring times relative to the main time constant
static const float rideAmp[4] = { 1.f, 0.75f, 0.55f, 0.4f };

static void model_rd9(ModelState* s, float* out, const uint8_t n, const ModelCtx* c)
{
	const float fb = 660.f * c->tf;
	float inc[4], r[4], amp[4];
	for(uint8_t k = 0; k < 4; k++)
	{
		inc[k] = fb * rideRatio[k] * M_INV_FS;
		r[k] = decayRate(rideTau[k] * c->tau);
		amp[k] = rideAmp[k] * partialGain(inc[k]);
	}
	uint32_t minc[6];
	ringMetal_set(minc, 1.8f * fb);
	const SvfCoef washHp = svf_coef(5000.f * c->tf, 1.2f);
	const SvfCoef pingBp = svf_coef(6500.f * c->tf, 0.8f);
	const float rW = decayRate(c->tau), rPing = decayRate(0.006f);
	const float pingAmt = 0.5f + 0.5f * c->velo;
	float w = s->click, ping = s->flt[4], low;
	for(uint8_t i = 0; i < n; i++)
	{
		float bell = 0.f;
		for(uint8_t k = 0; k < 4; k++)
		{
			s->ph[k] = phaseAdd(s->ph[k], inc[k]);
			bell += amp[k] * s->env[k] * sinCycle(s->ph[k]);
			s->env[k] *= r[k];
		}
		const float nz = rngFloat(s);
		const float metal = 0.7f * ringMetal_tick(s, minc) + 0.3f * nz;
		const float wash = svf_high(&washHp, &s->flt[0], metal) * w;
		const float pb = svf_tick(&pingBp, &s->flt[2], nz, &low);
		const float o = 0.14f * bell + 0.47f * wash + pingAmt * ping * pb;
		out[i] = softSat(0.7f * o);
		w *= rW;
		ping *= rPing;
	}
	s->click = w; s->flt[4] = ping;
}

//-----------------------------------------------------------------------
// The acoustic instruments are modal: each mode of the bar, kettle, skin or
// wood is a sine at a ratio of the first mode's frequency with its own level
// and decay, struck by a short burst of band passed noise (the mallet, stick
// or hand). The upper modes are lower on soft hits.
//-----------------------------------------------------------------------
#define MAX_MODES	8

typedef struct { float ratio, amp, tau; } Mode;	// tau relative to the main time constant

typedef struct
{
	float	f;				// frequency of the first mode at the original tuning [Hz]
	uint8_t	num;			// number of modes
	Mode	m[MAX_MODES];
	float	pairHz;			// > 0: two instruments that play together, this many Hz apart (num <= 4)
	float	bend, bendVelo;	// the pitch starts higher by bend + bendVelo * velocity (skin tension)
	float	bendTau;		// and settles with this time constant [s]
	float	nzF, nzK;		// strike: band pass of the noise burst [Hz], 1/Q
	float	nzTau, nzAmp;	// strike: time constant [s] and level
	float	soft;			// a soft hit lowers the upper modes by up to this much
} ModalDef;

static void model_modal(ModelState* s, float* out, const uint8_t n, const ModelCtx* c, const ModalDef* d)
{
	const uint8_t num = d->num;
	const uint8_t paired = d->pairHz > 0.f;
	const float bend = d->bend + d->bendVelo * c->velo;
	const float bright = 1.f - d->soft * (1.f - c->velo);
	const float f = d->f * c->tf;
	const float fA = paired ? f - 0.5f * d->pairHz : f;
	const float fB = f + 0.5f * d->pairHz;
	float incA[MAX_MODES], incB[MAX_MODES], r[MAX_MODES], amp[MAX_MODES];
	for(uint8_t k = 0; k < num; k++)
	{
		incA[k] = d->m[k].ratio * fA * M_INV_FS;
		incB[k] = d->m[k].ratio * fB * M_INV_FS;
		r[k] = decayRate(d->m[k].tau * c->tau);
		// muted where the start of the bend would alias
		amp[k] = d->m[k].amp * (k ? bright : 1.f) * partialGain(incB[k] * (1.f + bend));
	}
	const float rB = decayRate(d->bendTau);
	const float rN = decayRate(d->nzTau);
	const SvfCoef bp = svf_coef(d->nzF * c->tf, d->nzK);
	const float nzAmp = d->nzAmp * (0.4f + 0.6f * c->velo);
	float b = s->env[8], e = s->env[9];
	for(uint8_t i = 0; i < n; i++)
	{
		const float pm = 1.f + bend * b;
		b *= rB;
		float x = 0.f;
		for(uint8_t k = 0; k < num; k++)
		{
			s->ph[k] = phaseAdd(s->ph[k], incA[k] * pm);
			float y = sinCycle(s->ph[k]);
			if(paired)
			{
				// the second one a little softer: the beats do not cancel completely
				s->ph[k + 4] = phaseAdd(s->ph[k + 4], incB[k] * pm);
				y += 0.8f * sinCycle(s->ph[k + 4]);
			}
			x += amp[k] * s->env[k] * y;
			s->env[k] *= r[k];
		}
		out[i] = x + nzAmp * e * svf_band(&bp, &s->flt[0], rngFloat(s));
		e *= rN;
	}
	s->env[8] = b; s->env[9] = e;
}

//-----------------------------------------------------------------------
// Gamelan. SRN saron: a thick bronze bar on a box, struck with a hard mallet;
// its modes after measured saron spectra (W. A. Sethares, "Tuning, Timbre,
// Spectrum, Scale"). GND gender: thin keys over tube resonators, padded
// mallets, played as the Balinese pair pengumbang and pengisep whose small
// detuning beats (ombak). BNG bonang: a small bossed kettle gong, its
// fundamental split into a slow beating doublet.
//-----------------------------------------------------------------------
static const ModalDef saron =
{
	523.25f, 5,
	{ { 1.f, 1.f, 1.f }, { 2.39f, 0.10f, 0.30f }, { 2.76f, 0.42f, 0.42f }, { 4.75f, 0.22f, 0.20f }, { 5.96f, 0.10f, 0.12f } },
	0.f, 0.f, 0.f, 0.01f,
	3200.f, 0.9f, 0.0015f, 0.35f, 0.5f
};

static const ModalDef gender =
{
	261.63f, 3,
	{ { 1.f, 1.f, 1.f }, { 2.756f, 0.08f, 0.30f }, { 5.404f, 0.025f, 0.15f } },
	5.5f, 0.f, 0.f, 0.01f,
	900.f, 1.2f, 0.003f, 0.08f, 0.6f
};

static const ModalDef bonang =
{
	523.25f, 6,
	{ { 1.f, 1.f, 1.f }, { 1.0026f, 0.30f, 0.85f }, { 1.52f, 0.08f, 0.20f }, { 2.70f, 0.28f, 0.40f }, { 3.88f, 0.10f, 0.22f }, { 5.20f, 0.05f, 0.12f } },
	0.f, 0.f, 0.f, 0.01f,
	1600.f, 1.0f, 0.002f, 0.15f, 0.5f
};

//-----------------------------------------------------------------------
// Hand drums. The modes of an ideal circular membrane (ratios of the Bessel
// zeros) shifted by the air in the body, a short rise of the pitch at the
// impact (the skin stretches) and the hand as the strike. DJB djembe bass:
// the open hand in the middle sounds the Helmholtz resonance of the goblet
// (65-80 Hz). DJT djembe tone: flat fingers at the edge, the (1,1) mode and
// the ones around it (300-420 Hz). DJS djembe slap: relaxed fingers, a crack
// of the higher modes and the fingers (700-1000 Hz and up). CGO conga open
// tone, CGS conga slap.
//-----------------------------------------------------------------------
static const ModalDef djembeBass =
{
	70.f, 4,
	{ { 1.f, 1.f, 1.f }, { 3.30f, 0.22f, 0.30f }, { 5.43f, 0.10f, 0.22f }, { 7.27f, 0.05f, 0.16f } },
	0.f, 0.06f, 0.06f, 0.025f,
	500.f, 1.4f, 0.006f, 0.45f, 0.5f
};

static const ModalDef djembeTone =
{
	380.f, 8,
	{ { 1.f, 1.f, 1.f }, { 0.627f, 0.25f, 0.55f }, { 1.340f, 0.50f, 0.75f }, { 1.441f, 0.12f, 0.45f },
	  { 1.665f, 0.30f, 0.55f }, { 1.831f, 0.16f, 0.45f }, { 1.980f, 0.14f, 0.40f }, { 0.184f, 0.14f, 0.90f } },
	0.f, 0.02f, 0.03f, 0.02f,
	2200.f, 1.0f, 0.003f, 0.20f, 0.5f
};

static const ModalDef djembeSlap =
{
	380.f, 8,
	{ { 1.f, 0.45f, 0.55f }, { 1.340f, 0.55f, 0.50f }, { 1.665f, 0.60f, 0.45f }, { 1.980f, 0.55f, 0.40f },
	  { 2.197f, 0.35f, 0.32f }, { 2.289f, 0.45f, 0.35f }, { 2.547f, 0.30f, 0.28f }, { 2.593f, 0.30f, 0.26f } },
	0.f, 0.03f, 0.04f, 0.012f,
	2800.f, 0.8f, 0.006f, 0.70f, 0.6f
};

static const ModalDef congaOpen =
{
	250.f, 7,
	{ { 1.f, 1.f, 1.f }, { 1.52f, 0.40f, 0.60f }, { 1.99f, 0.30f, 0.50f }, { 2.45f, 0.16f, 0.38f },
	  { 2.92f, 0.10f, 0.30f }, { 3.38f, 0.06f, 0.24f }, { 0.50f, 0.10f, 0.50f } },
	0.f, 0.03f, 0.03f, 0.015f,
	1500.f, 1.1f, 0.003f, 0.18f, 0.5f
};

static const ModalDef congaSlap =
{
	250.f, 8,
	{ { 1.f, 0.40f, 0.50f }, { 1.52f, 0.50f, 0.45f }, { 1.99f, 0.55f, 0.40f }, { 2.45f, 0.50f, 0.35f },
	  { 2.92f, 0.45f, 0.30f }, { 3.38f, 0.35f, 0.26f }, { 3.85f, 0.25f, 0.22f }, { 4.40f, 0.18f, 0.20f } },
	0.f, 0.03f, 0.04f, 0.010f,
	3200.f, 0.8f, 0.005f, 0.75f, 0.6f
};

//-----------------------------------------------------------------------
// Taiko, struck with the bachi. Two thick cowhide heads coupled by the air in
// the body; the pitch starts higher and settles as the hit fades (the
// tension of the skin rises with its swing), more on hard hits. ODK odaiko,
// the big drum (49 Hz); NGD nagado-daiko (98 Hz); SHM shime-daiko, small and
// laced very tight (420 Hz); TKR the bachi on the wooden rim ("ka"): a few
// short resonances of the wood.
//-----------------------------------------------------------------------
static const ModalDef odaiko =
{
	49.f, 8,
	{ { 1.f, 1.f, 1.f }, { 1.38f, 0.40f, 0.55f }, { 1.66f, 0.35f, 0.60f }, { 2.17f, 0.22f, 0.40f },
	  { 2.68f, 0.14f, 0.30f }, { 3.20f, 0.09f, 0.22f }, { 4.10f, 0.06f, 0.15f }, { 5.30f, 0.04f, 0.10f } },
	0.f, 0.06f, 0.08f, 0.07f,
	1100.f, 1.2f, 0.006f, 0.30f, 0.5f
};

static const ModalDef nagado =
{
	98.f, 8,
	{ { 1.f, 1.f, 1.f }, { 1.38f, 0.40f, 0.55f }, { 1.66f, 0.35f, 0.60f }, { 2.17f, 0.22f, 0.40f },
	  { 2.68f, 0.14f, 0.30f }, { 3.20f, 0.09f, 0.22f }, { 4.10f, 0.06f, 0.15f }, { 5.30f, 0.04f, 0.10f } },
	0.f, 0.05f, 0.06f, 0.05f,
	1400.f, 1.1f, 0.005f, 0.32f, 0.5f
};

static const ModalDef shime =
{
	420.f, 6,
	{ { 1.f, 1.f, 1.f }, { 1.594f, 0.45f, 0.60f }, { 2.136f, 0.32f, 0.50f }, { 2.296f, 0.12f, 0.40f },
	  { 2.653f, 0.20f, 0.35f }, { 2.918f, 0.10f, 0.30f } },
	0.f, 0.02f, 0.02f, 0.01f,
	3500.f, 0.9f, 0.002f, 0.40f, 0.5f
};

static const ModalDef taikoRim =
{
	1150.f, 5,
	{ { 1.f, 1.f, 1.f }, { 1.626f, 0.60f, 0.70f }, { 2.304f, 0.45f, 0.50f }, { 3.391f, 0.25f, 0.35f }, { 0.365f, 0.35f, 0.80f } },
	0.f, 0.f, 0.f, 0.01f,
	4500.f, 0.8f, 0.0012f, 0.60f, 0.4f
};

//-----------------------------------------------------------------------
// GNG: gong ageng, the large bossed gong. Its two strongest modes are nearly,
// but not exactly, an octave apart. The gong's nonlinearity makes an octave of
// the fundamental that beats with the second mode a few times per second
// (B. J. Krueger, K. L. Gee, D. W. Grimshaw, JASA 128, EL8, 2010), and the
// second mode builds up over the first half second. Asymmetric modes above
// them, and a padded beater.
//-----------------------------------------------------------------------
static const Mode gongMode[6] =
{
	{ 1.f, 1.f, 1.f }, { 2.035f, 0.45f, 0.75f }, { 2.68f, 0.10f, 0.40f },
	{ 3.06f, 0.09f, 0.35f }, { 3.77f, 0.06f, 0.25f }, { 4.43f, 0.04f, 0.18f }
};

static void model_gng(ModelState* s, float* out, const uint8_t n, const ModelCtx* c)
{
	const float f = 65.41f * c->tf;
	float inc[6], r[6], amp[6];
	for(uint8_t k = 0; k < 6; k++)
	{
		inc[k] = gongMode[k].ratio * f * M_INV_FS;
		r[k] = decayRate(gongMode[k].tau * c->tau);
		amp[k] = gongMode[k].amp * partialGain(inc[k]);
	}
	const float nl = (0.25f + 0.35f * c->velo) * partialGain(2.f * inc[0]);
	const float rSwell = decayRate(0.25f);
	const float rT = decayRate(0.03f);
	const float lpT = onePole(250.f);
	const float thump = 0.35f * (0.4f + 0.6f * c->velo);
	float sw = s->env[8], th = s->env[9], lp = s->flt[0];
	for(uint8_t i = 0; i < n; i++)
	{
		for(uint8_t k = 0; k < 6; k++)
			s->ph[k] = phaseAdd(s->ph[k], inc[k]);
		const float a0 = s->env[0];
		const float q2 = 2.f * s->ph[0];
		float x = amp[0] * a0 * sinCycle(s->ph[0]) + nl * a0 * a0 * sinCycle(q2 - (float)(int32_t)q2);
		x += amp[1] * (1.f - sw) * s->env[1] * sinCycle(s->ph[1]);
		for(uint8_t k = 2; k < 6; k++)
			x += amp[k] * s->env[k] * sinCycle(s->ph[k]);
		for(uint8_t k = 0; k < 6; k++)
			s->env[k] *= r[k];
		sw *= rSwell;
		lp += lpT * (rngFloat(s) - lp);
		out[i] = x + thump * th * lp;
		th *= rT;
	}
	s->env[8] = sw; s->env[9] = th; s->flt[0] = lp;
}

//-----------------------------------------------------------------------
// tube saturator on the PWM knob: an asymmetric triode style curve for even
// harmonics, a low shelf in front of it for body, a DC blocker and a soft
// high cut that closes a little as the drive goes up.
//-----------------------------------------------------------------------
static void tube_block(ModelState* s, float* x, const uint8_t n, const float amt)
{
	const float drive = 1.f + 7.f * amt * amt;
	const float bias = 0.22f * amt;
	const float sb = softSat(bias);
	// keeps the peaks of a hard hit about 1 dB below full scale at full drive
	const float norm = (1.f - 0.2f * amt) / (1.f + sb);
	const float shelf = 0.5f * amt;
	const float lowC = onePole(180.f);
	const float lpC = onePole(16000.f * (1.f - 0.5f * amt));
	const float dcR = 1.f - 2.f * M_PI_F * 10.f * M_INV_FS;
	float low = s->satLow, lp = s->satLp, dx = s->satX, dy = s->satY;
	for(uint8_t i = 0; i < n; i++)
	{
		low += lowC * (x[i] - low);
		const float in = x[i] + shelf * low;
		const float y = (softSat(drive * in + bias) - sb) * norm;
		// DC blocker: the asymmetric curve shifts the level of a decaying hit
		const float hp = y - dx + dcR * dy;
		dx = y;
		dy = hp;
		lp += lpC * (hp - lp);
		x[i] = lp;
	}
	s->satLow = low; s->satLp = lp; s->satX = dx; s->satY = dy;
}

//-----------------------------------------------------------------------
void model_trigger(OscInfo* osc, const uint8_t vel)
{
	if(!osc_isModel(osc->waveform))
		return;

	ModelState* s = &osc->model;
	if(!s->seeded)
	{
		s->rng = GetRngValue() | 1u;
		for(uint8_t k = 0; k < 6; k++)
			s->sq[k] = rngNext(s);
		s->seeded = 1;
	}

	s->velo = vel * (1.f / 127.f);
	s->drift = 1.f + 0.0025f * rngFloat(s);	// about +-2 cent per hit, like analog drift
	// the reset below would jump from the last audible sample to zero and
	// click, so the previous hit fades out over 1.5 ms instead. lastOut is taken
	// before the amp EG, so it is scaled by the EG level of the last block.
	s->declick = s->lastOut * osc->modelEnv;
	s->count = 0;
	s->burst = 0;
	s->nextBurst = 0;
	s->click = 0.f;
	for(uint8_t k = 0; k < 8; k++)
		s->ph[k] = 0.f;
	for(uint8_t k = 0; k < 10; k++)
		s->env[k] = 1.f;
	// filter states restart from zero, the 808 metal keeps running like the
	// free running analog oscillators
	for(uint8_t k = 0; k < 10; k++)
		s->flt[k] = 0.f;

	switch(osc->waveform)
	{
	case MODEL_SD9:
		s->click = 1.f;		// pitch drop of the VCOs
		break;
	case MODEL_CP8:
	case MODEL_CP9:
		s->env[0] = 0.f;	// bursts and tail start in the block
		s->env[1] = 0.f;
		break;
	case MODEL_RD9:
		s->click = 1.f;		// wash level
		s->flt[4] = 1.f;	// stick ping
		break;
	default:
		break;
	}
}

//-----------------------------------------------------------------------
// output level of each instrument: on a hard hit at the preset settings the
// drums peak at about -1 to -2 dBFS, hats and cymbals at about -4 dBFS
static const float modelLevel[NUM_ANALOG_MODELS] =
{
	0.899f, 0.889f, 0.706f, 0.698f,	// BD8 BD9 SD8 SD9
	0.931f, 0.931f, 0.942f, 0.889f, 0.820f, 0.869f,	// LT8 MT8 HT8 LT9 MT9 HT9
	0.910f, 0.899f, 0.792f,	// LC8 MC8 HC8
	0.802f, 0.802f, 0.714f,	// RS8 RS9 CL8
	1.393f, 1.132f, 0.430f, 0.774f,	// CP8 CP9 MA8 CB8
	2.084f, 1.119f, 1.094f, 0.723f, 1.057f,	// HH8 HH9 CY8 CR9 RD9
	0.586f, 0.440f, 0.476f, 0.499f,	// SRN GND BNG GNG
	0.918f, 0.365f, 0.256f, 0.493f, 0.297f,	// DJB DJT DJS CGO CGS
	0.466f, 0.455f, 0.455f, 0.415f,	// ODK NGD SHM TKR
};

//-----------------------------------------------------------------------
void model_calcBlock(OscInfo* osc, int16_t* buf, const uint8_t size, const float gain, const float sat)
{
	ModelState* s = &osc->model;
	if(s->drift == 0.f)
		s->drift = 1.f;	// not triggered yet

	ModelCtx c;
	// coarse 0 on the display (C4, 261.6 Hz with the step note at C4) is the original tuning
	c.tf = osc->phaseInc * PHASE_TO_HZ * s->drift * (1.f / 261.6256f);
	// the voice's amp EG (a gate for these instruments) runs for 1/decay blocks
	const float d = osc->modelDecay;
	c.T = (d > (float)OUTPUT_DMA_SIZE / (10.f * M_FS)) ? (float)OUTPUT_DMA_SIZE / (d * M_FS) : 10.f;
	c.tau = c.T * (1.f / LN_60DB);
	c.velo = s->velo;
	c.t0 = s->count;

	float out[size];
	switch(osc->waveform)
	{
	case MODEL_BD8:	model_bd8(s, out, size, &c);	break;
	case MODEL_BD9:	model_bd9(s, out, size, &c);	break;
	case MODEL_SD8:	model_sd8(s, out, size, &c);	break;
	case MODEL_SD9:	model_sd9(s, out, size, &c);	break;
	case MODEL_LT8:
	case MODEL_MT8:
	case MODEL_HT8:
	case MODEL_LC8:
	case MODEL_MC8:
	case MODEL_HC8:
		model_tom808(s, out, size, &c, &tom808[(osc->waveform <= MODEL_HT8) ? (osc->waveform - MODEL_LT8) : (3 + osc->waveform - MODEL_LC8)]);
		break;
	case MODEL_LT9:
	case MODEL_MT9:
	case MODEL_HT9:
		model_tom909(s, out, size, &c, tom909[osc->waveform - MODEL_LT9]);
		break;
	case MODEL_RS8:	model_rim(s, out, size, &c, &rim808);		break;
	case MODEL_RS9:	model_rim(s, out, size, &c, &rim909);		break;
	case MODEL_CL8:	model_rim(s, out, size, &c, &clave808);	break;
	case MODEL_CP8:	model_clap(s, out, size, &c, &clap808);	break;
	case MODEL_CP9:	model_clap(s, out, size, &c, &clap909);	break;
	case MODEL_MA8:	model_ma8(s, out, size, &c);	break;
	case MODEL_CB8:	model_cb8(s, out, size, &c);	break;
	case MODEL_HH8:	model_hh8(s, out, size, &c);	break;
	case MODEL_HH9:	model_hh9(s, out, size, &c);	break;
	case MODEL_CY8:	model_cy8(s, out, size, &c);	break;
	case MODEL_CR9:	model_cr9(s, out, size, &c);	break;
	case MODEL_RD9:	model_rd9(s, out, size, &c);	break;
	case MODEL_SRN:	model_modal(s, out, size, &c, &saron);	break;
	case MODEL_GND:	model_modal(s, out, size, &c, &gender);	break;
	case MODEL_BNG:	model_modal(s, out, size, &c, &bonang);	break;
	case MODEL_GNG:	model_gng(s, out, size, &c);	break;
	case MODEL_DJB:	model_modal(s, out, size, &c, &djembeBass);	break;
	case MODEL_DJT:	model_modal(s, out, size, &c, &djembeTone);	break;
	case MODEL_DJS:	model_modal(s, out, size, &c, &djembeSlap);	break;
	case MODEL_CGO:	model_modal(s, out, size, &c, &congaOpen);	break;
	case MODEL_CGS:	model_modal(s, out, size, &c, &congaSlap);	break;
	case MODEL_ODK:	model_modal(s, out, size, &c, &odaiko);	break;
	case MODEL_NGD:	model_modal(s, out, size, &c, &nagado);	break;
	case MODEL_SHM:	model_modal(s, out, size, &c, &shime);	break;
	case MODEL_TKR:	model_modal(s, out, size, &c, &taikoRim);	break;
	default:
		memset(buf, 0, size * sizeof(int16_t));
		return;
	}

	const float lvl = modelLevel[osc->waveform - MODEL_BD8];
	for(uint8_t i = 0; i < size; i++)
		out[i] *= lvl;

	if((s->declick > 1e-4f) || (s->declick < -1e-4f))
	{
		const float r = decayRate(0.0015f);
		float dc = s->declick;
		for(uint8_t i = 0; i < size; i++)
		{
			out[i] += dc;
			dc *= r;
		}
		s->declick = dc;
	}
	else
		s->declick = 0.f;
	s->lastOut = out[size - 1];

	const float amt = clampf(sat, 0.f, 1.f);
	if(amt > 0.004f)
		tube_block(s, out, size, amt);
	else
		s->satLow = s->satLp = s->satX = s->satY = 0.f;

	for(uint8_t i = 0; i < size; i++)
		buf[i] = toInt16(kneeLimit(out[i] * gain) * 32767.f);
	s->count += size;
}
