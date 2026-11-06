/*
 * VoiceFx.c
 *
 * The voice FX, see VoiceFx.h. Amount and the third knob by type:
 * - cmp compressor: amount lowers the threshold (0 to -40 dB) and raises the
 *   ratio (1:1 to 10:1). The make-up gain gives back all the gain a full-scale
 *   hit loses, so the voice keeps its peak level and gets denser and louder as
 *   the amount goes up; a soft limit catches the transients that pass the
 *   attack. Time sets attack and release together (0.1 ms / 30 ms to 5 ms /
 *   600 ms).
 * - tub tube: an asymmetric soft saturation, even harmonics.
 * - fld wavefolder: folds the wave back beyond full scale.
 * - clp clipper: hard clipping.
 *   For the three: amount is the drive (up to +24 dB, about a third of it
 *   taken back at the output), tone a low pass after the distortion (1 kHz
 *   to open).
 * - rng ring modulator: amount mixes the ring modulated signal in, frequency
 *   is the carrier (30 Hz to 6 kHz).
 * The voice's volume (its MIX page volume and its fader) comes at the end of
 * the voice, so the FX take it back out first: their threshold and drive stay
 * the same at any volume, and the volume still sets the level.
 * The send to the FX bus follows the FX, with a squared taper (64 is -12 dB).
 * Nothing here runs from ITCM.
 *
 * Part of a custom firmware for the Erica Synths LXR-02 (not an official
 * Erica Synths release). Added 2026.
 * SPDX-License-Identifier: GPL-3.0-only
 *
 * Licensed under the GNU General Public License version 3, with the
 * additional permission and terms in NOTICE.md. See LICENSE and NOTICE.md in
 * the root of the repository.
 */

#include "Soundengine/DSPAudio/VoiceFx.h"
#include "Soundengine/DSPAudio/VoiceSettings.h"
#include "Soundengine/DSPAudio/mixer.h"
#include "Soundengine/DSPAudio/DrumVoice.h"
#include "Soundengine/DSPAudio/Snare.h"
#include "Soundengine/DSPAudio/CymbalVoice.h"
#include "Soundengine/DSPAudio/HiHat.h"
#include "Soundengine/config.h"
#include <math.h>

#define FX_FS		REAL_FS
#define FX_INV_FS	(1.f / REAL_FS)
#define PI_F		3.14159265358979f

typedef struct
{
	uint8_t		type;		// VFX_OFF .. VFX_RING
	float		amt;		// amount [0:1]
	float		par;		// time, tone or frequency [0:1]
	float		send;		// gain of the send to the FX bus
	// compressor
	float		env;		// level detector
	// distortion
	float		lp;			// tone low pass
	float		dcX, dcY;	// DC blocker of the tube
	// ring modulator
	float		ph;			// carrier phase [0:1)
} VoiceFxState;

static INDTCMZ VoiceFxState voiceFx[6];

//-----------------------------------------------------------------------
void voiceFx_setType(uint8_t voice, uint8_t type)
{
	if(voice > 5) return;
	voiceFx[voice].type = (type < NUM_VFX) ? type : VFX_OFF;
}

void voiceFx_setAmount(uint8_t voice, uint8_t value)
{
	if(voice > 5) return;
	voiceFx[voice].amt = (value > 127 ? 127 : value) * (1.f / 127.f);
}

void voiceFx_setParam(uint8_t voice, uint8_t value)
{
	if(voice > 5) return;
	voiceFx[voice].par = (value > 127 ? 127 : value) * (1.f / 127.f);
}

void voiceFx_setSend(uint8_t voice, uint8_t value)
{
	if(voice > 5) return;
	const float s = (value > 127 ? 127 : value) * (1.f / 127.f);
	voiceFx[voice].send = s * s;
}

//-----------------------------------------------------------------------
// helpers
//-----------------------------------------------------------------------
static inline float clamp1(const float x)
{
	return x < -1.f ? -1.f : (x > 1.f ? 1.f : x);
}

/** log2(x) for x > 0, within 0.01 */
static inline float fastLog2(const float x)
{
	union { float f; uint32_t i; } v = { x };
	// exponent - 1, plus a fit of log2(m) + 1 for the mantissa 1 <= m < 2
	const float e = (float)(int32_t)((v.i >> 23) & 0xff) - 128.f;
	v.i = (v.i & 0x007fffffu) | 0x3f800000u;
	const float m = v.f;
	return e + (-0.34484843f * m + 2.02466578f) * m - 0.67487759f;
}

/** 2^x for -60 < x < 60, within 0.01 % */
static inline float fastExp2(const float x)
{
	const float fl = floorf(x);
	const float f = x - fl;
	union { float f; uint32_t i; } v;
	v.i = (uint32_t)((int32_t)fl + 127) << 23;
	return v.f * (1.f + f * (0.6960656421f + f * (0.2244265138f + f * 0.0794842042f)));
}

/** smooth saturation, tanh(x) within 0.3 % for |x| <= 3, exactly +-1 beyond */
static inline float softSat(float x)
{
	x = x < -3.f ? -3.f : (x > 3.f ? 3.f : x);
	const float x2 = x * x;
	return x * (27.f + x2) / (27.f + 9.f * x2);
}

/** sin(2 pi q) for a phase 0 <= q < 1, within 0.1 % */
static inline float sinCycle(const float q)
{
	const float x = 2.f * q - 1.f;
	float y = 4.f * x * (1.f - fabsf(x));
	y = 0.225f * (y * fabsf(y) - y) + y;
	return -y;
}

/** transparent up to 0.8, then a soft knee to full scale (catches the
 * transients that pass a slow attack with the make-up gain) */
static inline float kneeLimit(const float x)
{
	const float ax = fabsf(x);
	if(ax <= 0.8f)
		return x;
	const float y = 0.8f + 0.2f * softSat((ax - 0.8f) * 5.f);
	return x < 0.f ? -y : y;
}

static inline int16_t toInt16(const float x)
{
	const float y = x * 32767.f;
	if(y > 32767.f) return 32767;
	if(y < -32767.f) return -32767;
	return (int16_t)y;
}

//-----------------------------------------------------------------------
/** the volume a voice applies at its end: the volume knob and the fader */
static float voiceFx_channelVolume(const uint8_t voice)
{
	switch(voice)
	{
	case 0:
	case 1:
	case 2:		return voiceArray[voice].vol * (float)voiceArray[voice].FaderVolumeFilter.b;
	case 3:		return snareVoice.vol * (float)snareVoice.FaderVolumeFilter.b;
	case 4:		return cymbalVoice.vol * (float)cymbalVoice.FaderVolumeFilter.b;
	default:	return hatVoice.vol * (float)hatVoice.FaderVolumeFilter.b;
	}
}

//-----------------------------------------------------------------------
// vol is the channel volume of the voice (> 0), see voiceFx_process
static void voiceFx_compressor(VoiceFxState* s, int16_t* buf, const uint8_t size, const float vol)
{
	const float thr = -40.f * s->amt * (1.f / 6.0206f);	// threshold in log2 units (6.02 dB each)
	const float slope = 1.f - 1.f / (1.f + 9.f * s->amt);	// 1 - 1/ratio
	const float makeup = -thr * slope;					// all the gain lost at full scale
	const float atk = 1.f - expf(-FX_INV_FS / (0.0001f * powf(50.f, s->par)));
	const float rel = 1.f - expf(-FX_INV_FS / (0.03f * powf(20.f, s->par)));
	const float pre = 1.f / vol;
	float env = s->env;
	for(uint8_t i = 0; i < size; i++)
	{
		const float x = buf[i] * (1.f / 32768.f);
		const float ax = fabsf(x) * pre;
		env += ((ax > env) ? atk : rel) * (ax - env);
		const float over = fastLog2(env + 1e-6f) - thr;
		const float g = (over > 0.f) ? makeup - over * slope : makeup;
		buf[i] = toInt16(vol * kneeLimit(pre * x * fastExp2(g)));
	}
	s->env = env;
}

//-----------------------------------------------------------------------
static void voiceFx_distortion(VoiceFxState* s, int16_t* buf, const uint8_t size, const float vol)
{
	const float a = s->amt;
	const float drive = 1.f + 15.f * a * a;
	const float in = 1.f / (vol * 32768.f);
	// the distorted wave is near full scale: about the loudness of the dry hit
	const float out = vol * powf(drive, -0.35f);
	// tone: 1 kHz at 0, open from about 80 %
	const float fc = 1000.f * powf(64.f, s->par);
	const float lpC = (fc < 0.4f * FX_FS) ? 1.f - expf(-2.f * PI_F * fc * FX_INV_FS) : 1.f;
	float lp = s->lp;
	switch(s->type)
	{
	case VFX_TUBE:
	{
		// asymmetric curve: even harmonics; a DC blocker takes out the offset
		const float bias = 0.2f * a;
		const float sb = softSat(bias);
		const float norm = 1.f / (softSat(drive + bias) - sb);
		const float dcR = 1.f - 2.f * PI_F * 10.f * FX_INV_FS;
		float dx = s->dcX, dy = s->dcY;
		for(uint8_t i = 0; i < size; i++)
		{
			const float y = (softSat(drive * buf[i] * in + bias) - sb) * norm;
			const float hp = y - dx + dcR * dy;
			dx = y;
			dy = hp;
			lp += lpC * (hp - lp);
			buf[i] = toInt16(out * lp);
		}
		s->dcX = dx; s->dcY = dy;
	}
	break;

	case VFX_FOLD:
		for(uint8_t i = 0; i < size; i++)
		{
			// sin(pi/2 * x * drive): saturates up to full scale, folds beyond
			float q = 0.25f * drive * buf[i] * in;
			q -= floorf(q);
			lp += lpC * (sinCycle(q) - lp);
			buf[i] = toInt16(out * lp);
		}
		break;

	default:	// VFX_CLIP
		for(uint8_t i = 0; i < size; i++)
		{
			lp += lpC * (clamp1(drive * buf[i] * in) - lp);
			buf[i] = toInt16(out * lp);
		}
		break;
	}
	s->lp = lp;
}

//-----------------------------------------------------------------------
static void voiceFx_ring(VoiceFxState* s, int16_t* buf, const uint8_t size)
{
	const float inc = 30.f * powf(200.f, s->par) * FX_INV_FS;
	const float wet = s->amt;
	float ph = s->ph;
	for(uint8_t i = 0; i < size; i++)
	{
		const float x = buf[i] * (1.f / 32768.f);
		buf[i] = toInt16(x * (1.f - wet + wet * sinCycle(ph)));
		ph += inc;
		ph -= (float)(int32_t)ph;
	}
	s->ph = ph;
}

//-----------------------------------------------------------------------
static void voiceFx_send(const VoiceFxState* s, const int16_t* buf, const uint8_t size, int16_t* fxBus, const float panL, const float panR)
{
	const float gl = s->send * panL, gr = s->send * panR;
	for(uint8_t i = 0; i < size; i++)
	{
		int32_t l = fxBus[2 * i] + (int32_t)(buf[i] * gl);
		int32_t r = fxBus[2 * i + 1] + (int32_t)(buf[i] * gr);
		fxBus[2 * i] = (int16_t)(l > 32767 ? 32767 : (l < -32768 ? -32768 : l));
		fxBus[2 * i + 1] = (int16_t)(r > 32767 ? 32767 : (r < -32768 ? -32768 : r));
	}
}

//-----------------------------------------------------------------------
void voiceFx_process(uint8_t voice, int16_t* buf, uint8_t size, int16_t* fxBus, float panL, float panR)
{
	if(voice > 5) return;
	VoiceFxState* s = &voiceFx[voice];
	const float vol = voiceFx_channelVolume(voice);
	switch((vol > 0.001f) ? s->type : VFX_OFF)	// a silent voice: nothing to do
	{
	case VFX_COMPRESSOR:
		voiceFx_compressor(s, buf, size, vol);
		break;
	case VFX_TUBE:
	case VFX_FOLD:
	case VFX_CLIP:
		voiceFx_distortion(s, buf, size, vol);
		break;
	case VFX_RING:
		voiceFx_ring(s, buf, size);
		break;
	default:
		// off: the states restart from rest when the FX is switched on again
		s->env = s->lp = s->dcX = s->dcY = 0.f;
		break;
	}
	if((s->send > 0.f) && (mixer_audioRouting[voice] != MIXER_ROUTING_FX_BUS))
		voiceFx_send(s, buf, size, fxBus, panL, panR);
}
