/*
 * host_render.c - renders the LXR-02 drum voices on a PC, using the real
 * firmware DSP and MIDI-parameter code, so new sounds can be checked and
 * auditioned before flashing. Not part of the firmware.
 *
 * Script commands (one per line, '#' starts a comment):
 *   kit init                 load the firmware's built-in init kit
 *   kitfile <path>           load a kit file (8 byte name + 247 parameter bytes)
 *   savekit <path> <name>    write the current parameters as a kit file
 *   set <param> <value>      set a sound parameter (name like PAR_COARSE1 or index)
 *   preset <voice> <wave>    choose waveform <wave> (number) for voice 0..5 like the
 *                            menu does: an 808/909 instrument also gets its voice settings
 *   hit <sec> <voice> <note> <vel> [<param> <value> ...]
 *                            trigger voice 0..6 (5 = closed hat, 6 = open hat),
 *                            optionally with up to 8 parameter locks, played
 *                            like the sequencer plays a step with locks
 *   len <sec>                length of the next render
 *   stereo 0|1               mono (default) or stereo with the kit's pan
 *   out <file.wav>           render the queued hits and write a 16 bit WAV
 *
 * Part of a custom firmware for the Erica Synths LXR-02 (not an official
 * Erica Synths release). Added 2026.
 * SPDX-License-Identifier: GPL-3.0-only
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <xmmintrin.h>
#include <pmmintrin.h>
#include "stm32f7xx_hal.h"
#include "Parameters.h"
#include "Preset/InitKit.h"
#include "Soundengine/DSPAudio/DrumVoice.h"
#include "Soundengine/DSPAudio/Snare.h"
#include "Soundengine/DSPAudio/CymbalVoice.h"
#include "Soundengine/DSPAudio/HiHat.h"
#include "Soundengine/DSPAudio/modulationNode.h"
#include "Soundengine/DSPAudio/lfo.h"
#include "Soundengine/DSPAudio/ResonantFilter.h"
#include "Soundengine/MIDI/MidiParser.h"
#include "Soundengine/MIDI/MidiVoiceControl.h"
#include "Soundengine/MIDI/ParameterArray.h"
#include "Soundengine/MIDI/MidiMessages.h"
#include "Soundengine/DSPAudio/automationNode.h"
#include "menu/modelPresets.h"
#include "param_names.h"

#ifndef CC_2
#define CC_2 0xd6 /* frontPanelParser_ui.h: parameters above 127 */
#endif
#define BLOCK OUTPUT_DMA_SIZE
#define WAV_FS 44100

extern uint8_t activeParameters[NUM_PARAMS];
extern float host_faderVolume[7];

#define MAX_LOCKS 8	/* NUM_STEP_LOCKS of the sequencer */
typedef struct { double t; int voice, note, vel, numLocks; uint8_t lockNr[MAX_LOCKS], lockVal[MAX_LOCKS]; } Hit;
static Hit hits[4096];
/* the parameter lock nodes per track, like seq_automationNodes */
static AutomationNode lockNodes[7][MAX_LOCKS];
static int numHits = 0;
static double renderLen = 2.0;
static int stereo = 0;

static int paramIndex(const char *s)
{
	if (isdigit((unsigned char)s[0])) return atoi(s);
	for (int i = 0; host_paramNames[i].name; i++)
		if (!strcmp(host_paramNames[i].name, s)) return host_paramNames[i].idx;
	fprintf(stderr, "unknown parameter %s\n", s);
	exit(1);
}

/* same dispatch as preset_morph_tick() in presetManager.c */
static void setParam(int i, uint8_t val)
{
	MidiMsg msg = {0};
	activeParameters[i] = val;
	if (i < 128) {
		msg.status = MIDI_CC;
		msg.data1 = (uint8_t)((i + 1) & 0x7f);
	} else {
		msg.status = CC_2;
		msg.data1 = (uint8_t)((i - 128) & 0x7f);
	}
	msg.data2 = val;
	midiParser_ccHandler(msg, 1, 0, 0.f);
}

static void setParamCb(uint16_t parNr, uint8_t val) { setParam(parNr, val); }

/* the waveform parameter of each voice, as modelPreset_voiceOfWaveParam() sees them */
static const int waveParam[6] = { PAR_OSC_WAVE_DRUM1, PAR_OSC_WAVE_DRUM2, PAR_OSC_WAVE_DRUM3,
	PAR_OSC_WAVE_SNARE, PAR_WAVE1_CYM, PAR_WAVE1_HH };

static void applyKit(const uint8_t *params, int n)
{
	for (int i = 0; i < END_OF_SOUND_PARAMETERS; i++)
		setParam(i, i < n ? params[i] : 0);
}

static void engineInit(void)
{
	static float nullTarget[4];
	parameterArray_init();
	/* on the STM32 address 0 is readable memory, on a PC it is not */
	for (int i = 0; i < END_OF_SOUND_PARAMETERS; i++)
		if (!parameterArray[i].ptr) parameterArray[i].ptr = nullTarget;
	for (int i = 0; i < 6; i++) modNode_init(&velocityModulators[i]);
	initDrumVoice();
	Snare_init();
	HiHat_init();
	Cymbal_init();
	for (int i = 0; i < 7; i++) for (int k = 0; k < MAX_LOCKS; k++) autoNode_init(&lockNodes[i][k]);
}

/* a step lock stores a parameter like the sequencer does: number+1 below 128 */
static uint8_t lockCode(int par) { return (uint8_t)(par < 128 ? par + 1 : par); }

/* like seq_triggerVoice(): the step's locks first (all destinations, then
 * the values), then the note */
static void trigger(const Hit *h)
{
	for (int k = 0; k < MAX_LOCKS; k++)
		autoNode_setDestination(&lockNodes[h->voice][k], k < h->numLocks ? h->lockNr[k] : NO_AUTOMATION);
	for (int k = 0; k < h->numLocks; k++)
		autoNode_updateValue(&lockNodes[h->voice][k], h->lockVal[k]);
	voiceControl_noteOn((uint8_t)h->voice, (uint8_t)h->note, (uint8_t)h->vel);
}

/* one 64 sample block of all six voices, like mixer_calcNextSampleBlock() without FX */
static void renderBlock(float *outL, float *outR)
{
	modNode_resetTargets();
	modNode_reassignVeloMod();
	lfo_dispatchNextValue(&voiceArray[0].lfo);
	lfo_dispatchNextValue(&voiceArray[1].lfo);
	lfo_dispatchNextValue(&voiceArray[2].lfo);
	lfo_dispatchNextValue(&snareVoice.lfo);
	lfo_dispatchNextValue(&cymbalVoice.lfo);
	lfo_dispatchNextValue(&hatVoice.lfo);
	SVF_recalcFreq(&voiceArray[0].filter);
	SVF_recalcFreq(&voiceArray[1].filter);
	SVF_recalcFreq(&voiceArray[2].filter);
	SVF_recalcFreq(&snareVoice.filter);
	SVF_recalcFreq(&cymbalVoice.filter);
	SVF_recalcFreq(&hatVoice.filter);
	calcDrumVoiceAsync(0);
	calcDrumVoiceAsync(1);
	calcDrumVoiceAsync(2);
	Snare_calcAsync();
	Cymbal_calcAsync();
	HiHat_calcAsync();

	int16_t buf[BLOCK];
	for (int v = 0; v < 6; v++) {
		uint8_t pan;
		switch (v) {
		case 0: case 1: case 2: calcDrumVoiceSyncBlock(v, buf, BLOCK); pan = voiceArray[v].pan; break;
		case 3: Snare_calcSyncBlock(buf, BLOCK); pan = snareVoice.pan; break;
		case 4: Cymbal_calcSyncBlock(buf, BLOCK); pan = cymbalVoice.pan; break;
		default: HiHat_calcSyncBlock(buf, BLOCK); pan = hatVoice.pan; break;
		}
		float gl = 1.f, gr = 1.f;
		if (stereo) { gl = sqrtf((127 - pan) / 127.f); gr = sqrtf(pan / 127.f); }
		for (int i = 0; i < BLOCK; i++) {
			outL[i] += buf[i] * gl;
			outR[i] += buf[i] * gr;
		}
	}
}

static int cmpHit(const void *a, const void *b)
{
	double d = ((const Hit *)a)->t - ((const Hit *)b)->t;
	return d < 0 ? -1 : d > 0;
}

static void writeWav(const char *path, const int16_t *data, long frames, int ch)
{
	FILE *f = fopen(path, "wb");
	if (!f) { perror(path); exit(1); }
	uint32_t dataBytes = (uint32_t)(frames * ch * 2);
	uint32_t u32; uint16_t u16;
	fwrite("RIFF", 1, 4, f); u32 = 36 + dataBytes; fwrite(&u32, 4, 1, f);
	fwrite("WAVEfmt ", 1, 8, f); u32 = 16; fwrite(&u32, 4, 1, f);
	u16 = 1; fwrite(&u16, 2, 1, f); u16 = (uint16_t)ch; fwrite(&u16, 2, 1, f);
	u32 = WAV_FS; fwrite(&u32, 4, 1, f); u32 = WAV_FS * ch * 2; fwrite(&u32, 4, 1, f);
	u16 = (uint16_t)(ch * 2); fwrite(&u16, 2, 1, f); u16 = 16; fwrite(&u16, 2, 1, f);
	fwrite("data", 1, 4, f); fwrite(&dataBytes, 4, 1, f);
	fwrite(data, 2, (size_t)(frames * ch), f);
	fclose(f);
}

static void render(const char *path)
{
	long frames = (long)(renderLen * WAV_FS);
	long blocks = (frames + BLOCK - 1) / BLOCK;
	int ch = stereo ? 2 : 1;
	int16_t *out = calloc((size_t)(blocks * BLOCK * ch), sizeof(int16_t));
	qsort(hits, (size_t)numHits, sizeof(Hit), cmpHit);
	int next = 0, clipped = 0;
	float peak = 0;
	for (long b = 0; b < blocks; b++) {
		double t = (double)(b * BLOCK) / WAV_FS;
		while (next < numHits && hits[next].t <= t + 1e-9) {
			trigger(&hits[next]);
			next++;
		}
		float L[BLOCK] = {0}, Rr[BLOCK] = {0};
		renderBlock(L, Rr);
		for (int i = 0; i < BLOCK; i++) {
			float l = stereo ? L[i] : L[i];
			float r = Rr[i];
			if (fabsf(l) > peak) peak = fabsf(l);
			if (fabsf(l) > 32767.f) { clipped++; l = l > 0 ? 32767.f : -32768.f; }
			if (fabsf(r) > 32767.f) { r = r > 0 ? 32767.f : -32768.f; }
			if (stereo) { out[(b * BLOCK + i) * 2] = (int16_t)l; out[(b * BLOCK + i) * 2 + 1] = (int16_t)r; }
			else out[b * BLOCK + i] = (int16_t)l;
		}
	}
	writeWav(path, out, frames, ch);
	printf("%s: %.2f s, peak %.1f dBFS%s\n", path, renderLen, 20 * log10f(peak / 32768.f + 1e-9f),
	       clipped ? " (CLIPPED)" : "");
	free(out);
	numHits = 0;
}

int main(int argc, char **argv)
{
	if (argc < 2) { fprintf(stderr, "usage: %s script.txt\n", argv[0]); return 1; }
	/* the STM32 runs with flush-to-zero (soundengine_init) */
	_MM_SET_FLUSH_ZERO_MODE(_MM_FLUSH_ZERO_ON);
	_MM_SET_DENORMALS_ZERO_MODE(_MM_DENORMALS_ZERO_ON);
	FILE *f = fopen(argv[1], "r");
	if (!f) { perror(argv[1]); return 1; }
	engineInit();
	applyKit(&initKit[8], INIT_KIT_SIZE - 8);
	char line[512];
	while (fgets(line, sizeof line, f)) {
		char *c = strchr(line, '#'); if (c) *c = 0;
		char cmd[64] = "", a[256] = "", b[256] = "", cc[64] = "", d[64] = "";
		int n = sscanf(line, "%63s %255s %255s %63s %63s", cmd, a, b, cc, d);
		if (n <= 0) continue;
		if (!strcmp(cmd, "kit") && !strcmp(a, "init")) applyKit(&initKit[8], INIT_KIT_SIZE - 8);
		else if (!strcmp(cmd, "kitfile")) {
			FILE *k = fopen(a, "rb"); uint8_t kb[8 + END_OF_SOUND_PARAMETERS] = {0};
			if (!k) { perror(a); return 1; }
			size_t got = fread(kb, 1, sizeof kb, k); fclose(k);
			applyKit(kb + 8, (int)got - 8);
		}
		else if (!strcmp(cmd, "savekit")) {
			FILE *k = fopen(a, "wb"); char name[8] = {0};
			strncpy(name, b, 8); fwrite(name, 1, 8, k);
			fwrite(activeParameters, 1, END_OF_SOUND_PARAMETERS, k); fclose(k);
		}
		else if (!strcmp(cmd, "set")) setParam(paramIndex(a), (uint8_t)atoi(b));
		else if (!strcmp(cmd, "preset")) {
			const int v = atoi(a), w = atoi(b);
			if (v < 0 || v > 5) { fprintf(stderr, "preset: voice 0..5\n"); return 1; }
			setParam(waveParam[v], (uint8_t)w);
			modelPreset_apply((uint8_t)v, (uint8_t)w, setParamCb);
		}
		else if (!strcmp(cmd, "hit")) {
			if (n < 5) { fprintf(stderr, "hit: <sec> <voice> <note> <vel> [<param> <value> ...]\n"); return 1; }
			if (numHits < 4096) {
				Hit *h = &hits[numHits++];
				h->t = atof(a); h->voice = atoi(b); h->note = atoi(cc); h->vel = atoi(d);
				h->numLocks = 0;
				/* the locks after the first five words */
				char *tok = strtok(line, " \t\r\n");
				for (int w = 0; w < 5 && tok; w++) tok = strtok(NULL, " \t\r\n");
				while (tok) {
					char *val = strtok(NULL, " \t\r\n");
					if (!val) { fprintf(stderr, "hit: lock %s without a value\n", tok); return 1; }
					if (h->numLocks >= MAX_LOCKS) { fprintf(stderr, "hit: more than %d locks\n", MAX_LOCKS); return 1; }
					h->lockNr[h->numLocks] = lockCode(paramIndex(tok));
					h->lockVal[h->numLocks++] = (uint8_t)atoi(val);
					tok = strtok(NULL, " \t\r\n");
				}
			}
		}
		else if (!strcmp(cmd, "len")) renderLen = atof(a);
		else if (!strcmp(cmd, "stereo")) stereo = atoi(a);
		else if (!strcmp(cmd, "fader")) host_faderVolume[atoi(a)] = (float)atof(b);
		else if (!strcmp(cmd, "out")) render(a);
		else { fprintf(stderr, "unknown command: %s\n", cmd); return 1; }
	}
	fclose(f);
	return 0;
}
