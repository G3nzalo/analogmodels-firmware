/*
 * patfile_test.c - PC test of the pattern files with the extra parameter
 * locks: the real presetManager.c saves and loads patterns (pattern files and
 * tmp.pat) on a folder of the PC (ff_stdio.c). Also builds against the
 * original firmware (no lock pool), to compare the files both write. And the
 * kit files and tmp.snd with the kit extension (per-voice settings).
 * usage: patfile_test <empty folder>
 *        patfile_test dump <folder> <pattern slot>   prints the locks of a pattern file
 * PC only, not part of the firmware.
 *
 * Part of a custom firmware for the Erica Synths LXR-02 (not an official
 * Erica Synths release). Added 2026.
 * SPDX-License-Identifier: GPL-3.0-only
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include "Soundengine/Sequencer/sequencer.h"
#include "Soundengine/MIDI/MidiMessages.h"
#include "Preset/PresetManager.h"
#include "menu/menu.h"
#ifdef __has_include
#if __has_include("Soundengine/DSPAudio/VoiceSettings.h")
#include "Soundengine/DSPAudio/VoiceSettings.h"
#endif
#endif

extern const char* ffhost_root;
extern char savefileNames[NUM_KIT_SLOTS][NUM_SAVEFILE_NAMES][8];
FRESULT preset_saveTmpPat(void);
FRESULT preset_LoadTmpPat(void);
FRESULT preset_saveTmpKit(void);

static int fails = 0, checks = 0;
#define CHECK(cond, ...) do { checks++; if (cond) printf("  ok    "); else { printf("  FAIL  "); fails++; } \
	printf(__VA_ARGS__); printf("\n"); } while (0)

#define PAT 3
static PatternSet* ps(void) { return get_seq_patternSet_p(); }
static uint8_t code(uint16_t par) { return (uint8_t)(par < 128 ? par + 1 : par); }

/* the pattern data the original format stores */
typedef struct {
	Step step[NUM_TRACKS][NUM_STEPS];
	StepTiming timing[NUM_TRACKS][NUM_STEPS];
	uint16_t mainSteps[NUM_BARS][NUM_TRACKS];
	uint8_t length[NUM_TRACKS], kitNr;
} PatSnap;

static void snap(uint8_t pat, PatSnap* s)
{
	memcpy(s->step, ps()->seq_subStepPattern[pat], sizeof s->step);
	memcpy(s->timing, ps()->seq_stepTiming[pat], sizeof s->timing);
	for (int b = 0; b < NUM_BARS; b++) memcpy(s->mainSteps[b], ps()->seq_mainSteps[b][pat], sizeof s->mainSteps[b]);
	memcpy(s->length, ps()->length[pat], sizeof s->length);
	s->kitNr = ps()->kitNr[pat];
}

static void scramble(uint8_t pat)
{
	memset(ps()->seq_subStepPattern[pat], 0x5a, sizeof(Step) * NUM_TRACKS * NUM_STEPS);
	ps()->kitNr[pat] = 99;
#ifdef LOCK_POOL_SIZE
	LockPool* p = seq_getLockPool(pat);
	for (int i = 0; i < 10; i++) { p->lock[i].track = 1; p->lock[i].step = (uint8_t)i; p->lock[i].nr = 77; p->lock[i].val = 1; }
	p->count = 10;
#endif
}

/* a pattern with notes, steps, timing and locks */
static void fill(uint8_t pat, int extras)
{
	seq_clearPattern(pat);
	for (int t = 0; t < NUM_TRACKS; t++)
		for (int s = 0; s < NUM_STEPS; s++) {
			Step* st = &ps()->seq_subStepPattern[pat][t][s];
			st->note = (uint8_t)(36 + (t * 7 + s) % 40);
			st->volume = (uint8_t)(((s * 5 + t) % 128) | ((s % 3 == 0) ? STEP_ACTIVE_MASK : 0));
			st->prob = (uint8_t)(50 + s % 51);
			ps()->seq_stepTiming[pat][t][s].flam = (uint8_t)(s % 6);
			ps()->seq_stepTiming[pat][t][s].shift = (uint8_t)((s + t) % 15);
		}
	for (int b = 0; b < NUM_BARS; b++)
		for (int t = 0; t < NUM_TRACKS; t++) ps()->seq_mainSteps[b][pat][t] = (uint16_t)(0x1111 * (b + 1) + t);
	for (int t = 0; t < NUM_TRACKS; t++) ps()->length[pat][t] = (uint8_t)(16 * (1 + t % 4));
	ps()->kitNr[pat] = 7;
	ps()->shuffle[pat] = 5;
	/* lock 1 and 2 */
	Step* a = &ps()->seq_subStepPattern[pat][0][0];
	a->param1Nr = code(PAR_COARSE1); a->param1Val = 70; a->param2Nr = code(PAR_VELOD1); a->param2Val = 20;
	Step* b = &ps()->seq_subStepPattern[pat][3][5];
	b->param1Nr = code(PAR_FILTER_FREQ_4); b->param1Val = 90;
	Step* c = &ps()->seq_subStepPattern[pat][6][63];
	c->param2Nr = code(PAR_PWM_6); c->param2Val = 33;
#ifdef LOCK_POOL_SIZE
	if (extras) {
		/* step 1 of drum 1 with all eight locks, a few more elsewhere */
		const uint16_t more[6] = { PAR_FINE1, PAR_PWM_1, PAR_VELOA1, PAR_MODAMNT1, PAR_FILTER_FREQ_1, PAR_RESO_1 };
		for (int k = 0; k < 6; k++) seq_setStepLock(pat, 0, 0, code(more[k]), (uint8_t)(10 + k));
		seq_setStepLock(pat, 3, 5, code(PAR_RESO_4), 11);
		seq_setStepLock(pat, 3, 5, code(PAR_VELOD4), 12);
		seq_setStepLock(pat, 6, 63, code(PAR_COARSE6), 13);
		seq_setStepLock(pat, 6, 63, code(PAR_VELOD6_OPEN), 14);
		seq_setStepLock(pat, 6, 63, code(PAR_WAVE1_HH), 15);
	}
#else
	(void)extras;
#endif
}

#ifdef LOCK_POOL_SIZE
/* all locks of a pattern, step by step, as one string */
static void lockList(uint8_t pat, char* out, size_t n)
{
	size_t len = 0;
	out[0] = 0;
	for (int t = 0; t < NUM_TRACKS; t++)
		for (int s = 0; s < NUM_STEPS; s++) {
			uint8_t nr[NUM_STEP_LOCKS], val[NUM_STEP_LOCKS];
			const uint8_t k = seq_getStepLocks(pat, (uint8_t)t, (uint8_t)s, nr, val);
			if (!k) continue;
			/* order independent: sort the locks of the step by parameter */
			for (int i = 0; i < k; i++)
				for (int j = i + 1; j < k; j++)
					if (nr[j] < nr[i]) { uint8_t x = nr[i]; nr[i] = nr[j]; nr[j] = x; x = val[i]; val[i] = val[j]; val[j] = x; }
			len += (size_t)snprintf(out + len, n - len, "%d/%d:", t, s);
			for (int i = 0; i < k; i++) len += (size_t)snprintf(out + len, n - len, " %d=%d", nr[i], val[i]);
			len += (size_t)snprintf(out + len, n - len, "; ");
		}
}

static int numLocks(uint8_t pat, uint8_t t, uint8_t s)
{
	uint8_t nr[NUM_STEP_LOCKS], val[NUM_STEP_LOCKS];
	return seq_getStepLocks(pat, t, s, nr, val);
}

static int truncate_file(const char* dir, const char* name, long len)
{
	char p[512];
	snprintf(p, sizeof p, "%s/%s", dir, name);
	return truncate(p, len);
}

/* writes a kit file: name, parameters and what follows them */
static void writeKit(const char* dir, const char* name, const uint8_t* params, const uint8_t* tail, size_t tailLen)
{
	char p[512];
	snprintf(p, sizeof p, "%s/%s", dir, name);
	FILE* f = fopen(p, "wb");
	fwrite("KitTest\0", 1, 8, f);
	fwrite(params, 1, END_OF_SOUND_PARAMETERS, f);
	if (tailLen) fwrite(tail, 1, tailLen, f);
	fclose(f);
}

static long fileSize(const char* dir, const char* name)
{
	char p[512];
	snprintf(p, sizeof p, "%s/%s", dir, name);
	FILE* f = fopen(p, "rb");
	if (!f) return -1;
	fseek(f, 0, SEEK_END);
	const long n = ftell(f);
	fclose(f);
	return n;
}
#endif

int main(int argc, char** argv)
{
	if (argc < 2) { fprintf(stderr, "usage: %s <empty folder> | dump <folder> <slot>\n", argv[0]); return 1; }
#ifdef LOCK_POOL_SIZE
	if (!strcmp(argv[1], "dump") && argc == 4) {
		/* a pattern file as the firmware loads it: its active steps and all locks */
		char list[16384];
		ffhost_root = argv[2];
		seq_init();
		const uint8_t slot = (uint8_t)atoi(argv[3]);
		if (preset_loadPatternSD(slot) != FR_OK) { fprintf(stderr, "can not load pattern %d\n", slot); return 1; }
		lockList(slot, list, sizeof list);
		printf("name %.8s kit %d pool %d\n", savefileNames[slot][PATTERN_NAMES], ps()->kitNr[slot], seq_getLockPool(slot)->count);
		printf("locks %s\n", list);
		return 0;
	}
#endif
	ffhost_root = argv[1];
	seq_init();
	memcpy(savefileNames[PAT][PATTERN_NAMES], "LockTest", 8);

#ifndef LOCK_POOL_SIZE
	/* original firmware: only write the pattern, the script compares the files */
	fill(PAT, 0);
	CHECK(preset_savePatternSD(PAT) == FR_OK, "original firmware: pattern %d saved", PAT);
	menu_playedPattern = PAT;
	CHECK(preset_saveTmpPat() == FR_OK, "original firmware: tmp.pat saved");
#else
	char want[16384], got[16384];
	PatSnap s0, s1;

	printf("1) save and load a pattern with up to 8 locks per step\n");
	fill(PAT, 1);
	CHECK(numLocks(PAT, 0, 0) == 8 && numLocks(PAT, 3, 5) == 3 && numLocks(PAT, 6, 63) == 4, "the pattern has 8, 3 and 4 locks on three steps");
	/* the snare's and the open hat's step had a free lock 2 or 1, the rest is in the pool */
	CHECK(seq_getLockPool(PAT)->count == 6 + 1 + 2, "9 of them in the pattern's lock pool (%d)", seq_getLockPool(PAT)->count);
	snap(PAT, &s0);
	lockList(PAT, want, sizeof want);
	CHECK(preset_savePatternSD(PAT) == FR_OK, "saved");
	scramble(PAT);
	CHECK(preset_loadPatternSD(PAT) == FR_OK, "loaded");
	snap(PAT, &s1);
	lockList(PAT, got, sizeof got);
	CHECK(!memcmp(&s0, &s1, sizeof s0), "notes, steps, timing, lengths and kit as saved");
	CHECK(!strcmp(want, got), "all locks as saved");
	if (strcmp(want, got)) printf("        want %s\n        got  %s\n", want, got);

	printf("\n2) a pattern file of the original firmware (no extra locks)\n");
	const long full = fileSize(argv[1], "03-LockT.pat");
	const long orig = full - (4 + 1 + 9 * 4);
	CHECK(full > 0 && truncate_file(argv[1], "03-LockT.pat", orig) == 0, "file cut to the original format (%ld of %ld bytes)", orig, full);
	scramble(PAT);
	CHECK(preset_loadPatternSD(PAT) == FR_OK, "loaded");
	snap(PAT, &s1);
	CHECK(!memcmp(&s0, &s1, sizeof s0), "notes, steps, lock 1 and 2 as saved");
	CHECK(seq_getLockPool(PAT)->count == 0 && numLocks(PAT, 0, 0) == 2 && numLocks(PAT, 3, 5) == 2 && numLocks(PAT, 6, 63) == 2,
		"no extra locks (lock pool empty, steps have their lock 1 and 2)");

	printf("\n3) a broken lock list in a file\n");
	fill(PAT, 0);
	CHECK(preset_savePatternSD(PAT) == FR_OK, "saved without extra locks");
	CHECK(fileSize(argv[1], "03-LockT.pat") == orig + 5, "the file has the tag and a count of 0 (%ld bytes)", fileSize(argv[1], "03-LockT.pat"));
	{
		/* replace the count by 250 and add entries: bad track, bad step, no parameter,
		 * a double, the parameter of lock 1, 7 extra locks on one step, then good ones */
		char p[512];
		snprintf(p, sizeof p, "%s/03-LockT.pat", argv[1]);
		FILE* f = fopen(p, "r+b");
		fseek(f, orig + 4, SEEK_SET);
		const uint8_t cnt = 250;
		fwrite(&cnt, 1, 1, f);
		const ExtraLock bad[] = {
			{ 9, 1, code(PAR_FINE1), 1 }, { 1, 70, code(PAR_FINE1), 1 }, { 1, 2, NO_AUTOMATION, 1 }, { 1, 2, 0, 1 },
			{ 2, 4, code(PAR_FINE3), 5 }, { 2, 4, code(PAR_FINE3), 6 },		/* double: the first one stays */
			{ 0, 0, code(PAR_COARSE1), 9 },									/* lock 1 of the step has it */
		};
		fwrite(bad, sizeof bad, 1, f);
		for (int k = 0; k < 7; k++) {
			const ExtraLock e = { 5, 9, (uint8_t)code((uint16_t)(PAR_FILTER_FREQ_6 + k)), (uint8_t)k };
			fwrite(&e, sizeof e, 1, f);
		}
		fclose(f);
	}
	scramble(PAT);
	CHECK(preset_loadPatternSD(PAT) == FR_OK, "loaded");
	LockPool* pool = seq_getLockPool(PAT);
	int okEntries = 1;
	for (int i = 0; i < pool->count; i++)
		if (pool->lock[i].track >= NUM_TRACKS || pool->lock[i].step >= NUM_STEPS || pool->lock[i].nr == 0 || pool->lock[i].nr == NO_AUTOMATION)
			okEntries = 0;
	CHECK(okEntries && pool->count == 1 + 6, "kept: the first of the double and 6 extra locks of the full step (%d)", pool->count);
	CHECK(numLocks(PAT, 2, 4) == 1 && numLocks(PAT, 5, 9) == 6 && numLocks(PAT, 0, 0) == 2, "steps: 1, 6 and the 2 own locks");
	uint8_t nr[NUM_STEP_LOCKS], val[NUM_STEP_LOCKS];
	seq_getStepLocks(PAT, 0, 0, nr, val);
	CHECK(val[0] == 70, "lock 1 of the step keeps its value (%d)", val[0]);
	seq_getStepLocks(PAT, 2, 4, nr, val);
	CHECK(val[0] == 5, "the double: the first one counts (%d)", val[0]);

	printf("\n4) a full lock pool\n");
	fill(PAT, 0);
	int added = 0;
	for (int t = 0; t < NUM_TRACKS && added < LOCK_POOL_SIZE; t++)
		for (int s = 0; s < NUM_STEPS && added < LOCK_POOL_SIZE; s++)
			for (int k = 0; k < NUM_STEP_LOCKS && added < LOCK_POOL_SIZE; k++) {
				Step* st = &ps()->seq_subStepPattern[PAT][t][s];
				const uint8_t before = seq_getLockPool(PAT)->count;
				seq_setStepLock(PAT, (uint8_t)t, (uint8_t)s, (uint8_t)(2 + k), (uint8_t)(t + s + k));
				if (seq_getLockPool(PAT)->count > before) added++;
				(void)st;
			}
	CHECK(seq_getLockPool(PAT)->count == LOCK_POOL_SIZE, "%d extra locks in the pattern", seq_getLockPool(PAT)->count);
	{
		/* a step with lock 1 and 2 used cannot take another one now */
		Step* st = &ps()->seq_subStepPattern[PAT][6][60];
		st->param1Nr = code(PAR_COARSE6); st->param2Nr = code(PAR_FINE6);
		CHECK(!seq_stepHasFreeLock(PAT, 6, 60) && !seq_setStepLock(PAT, 6, 60, code(PAR_PWM_6), 1), "pool full: no more extra locks");
		st->param1Nr = st->param2Nr = NO_AUTOMATION;
	}
	lockList(PAT, want, sizeof want);
	CHECK(preset_savePatternSD(PAT) == FR_OK, "saved");
	scramble(PAT);
	CHECK(preset_loadPatternSD(PAT) == FR_OK, "loaded");
	lockList(PAT, got, sizeof got);
	CHECK(!strcmp(want, got) && seq_getLockPool(PAT)->count == LOCK_POOL_SIZE, "all locks as saved");

	printf("\n5) tmp.pat (the pattern being played, saved in the background)\n");
	fill(PAT, 1);
	snap(PAT, &s0);
	lockList(PAT, want, sizeof want);
	menu_playedPattern = PAT;
	CHECK(preset_saveTmpPat() == FR_OK, "saved");
	scramble(PAT);
	CHECK(preset_LoadTmpPat() == FR_OK, "loaded");
	snap(PAT, &s1);
	lockList(PAT, got, sizeof got);
	CHECK(!memcmp(&s0, &s1, sizeof s0) && !strcmp(want, got), "pattern and all locks as saved");

	printf("\n6) copy, clear, backup buffer\n");
	fill(PAT, 1);
	seq_copyPattern(PAT, 9);
	lockList(PAT, want, sizeof want);
	lockList(9, got, sizeof got);
	CHECK(!strcmp(want, got), "copy pattern: the copy has all locks");
	seq_copyTrack(0, 2, PAT);
	CHECK(numLocks(PAT, 2, 0) == 8 && numLocks(PAT, 0, 0) == 8, "copy track: drum 3 has the 8 locks of drum 1 (%d)", numLocks(PAT, 2, 0));
	seq_copyExtraLocks(PAT, 0, 0, PAT, 0, 16);
	CHECK(numLocks(PAT, 0, 16) == 6, "copy a step's extra locks (bar copy): 6 (%d)", numLocks(PAT, 0, 16));
	seq_clearAutomation(2, PAT, 0);
	CHECK(numLocks(PAT, 2, 0) == 7, "clear automation 1: lock 1 gone (%d left)", numLocks(PAT, 2, 0));
	seq_clearAutomation(2, PAT, 1);
	CHECK(numLocks(PAT, 2, 0) == 0 && numLocks(PAT, 0, 0) == 8, "clear automation 2: lock 2 and the extra locks gone, drum 1 keeps its locks");
	seq_clearStepLocks(0, PAT, 16);
	CHECK(numLocks(PAT, 0, 16) == 0, "clear the locks of one step");
	lockList(PAT, want, sizeof want);
	seq_copyPatternToBackupBuffer(PAT);
	seq_clearTrack(0, PAT);
	CHECK(numLocks(PAT, 0, 0) == 0 && numLocks(PAT, 3, 5) == 3, "clear track: drum 1 without locks, the snare keeps its 3");
	seq_copyBackupBufferToPattern(PAT);
	lockList(PAT, got, sizeof got);
	CHECK(!strcmp(want, got), "the backup buffer brings the locks back (pattern change without saving)");
	seq_clearPattern(9);
	CHECK(seq_getLockPool(9)->count == 0 && numLocks(9, 0, 0) == 0, "clear pattern: no locks");

#ifdef NUM_KIT_EXT
	printf("\n7) kit files and tmp.snd with the kit extension\n");
	{
		const uint8_t K = 5;
		const char* kitFile = "05-KitTe.snd";	/* the slot and the first 5 letters of the name */
		uint8_t params[END_OF_SOUND_PARAMETERS], want[NUM_KIT_EXT], defaults[NUM_KIT_EXT];
		for (int i = 0; i < END_OF_SOUND_PARAMETERS; i++) params[i] = (uint8_t)((i * 7) % 128);
		for (int v = 0; v < 6; v++) {
			want[PAR_SCALE1 - PAR_BEGINNING_OF_KIT_EXT + v] = (uint8_t)(v % NUM_SCALES);
			want[PAR_VFX_TYPE1 - PAR_BEGINNING_OF_KIT_EXT + v] = (uint8_t)(v % NUM_VFX);
			want[PAR_VFX_AMT1 - PAR_BEGINNING_OF_KIT_EXT + v] = (uint8_t)(10 + v);
			want[PAR_VFX_PAR1 - PAR_BEGINNING_OF_KIT_EXT + v] = (uint8_t)(100 + v);
			want[PAR_VFX_SEND1 - PAR_BEGINNING_OF_KIT_EXT + v] = (uint8_t)(120 + v);
		}
		voiceSettings_defaults(defaults);
		memcpy(savefileNames[K][KIT_NAMES], "KitTest", 8);
		memcpy(mainParameters[K], params, sizeof params);
		memcpy(mainKitExt[K], want, sizeof want);
		CHECK(preset_saveDrumsetSD(K, 0) == FR_OK && fileSize(argv[1], kitFile) > 0, "kit %d saved (%s)", K, kitFile);
		CHECK(fileSize(argv[1], kitFile) == 8 + END_OF_SOUND_PARAMETERS + 5 + NUM_KIT_EXT,
			"name, %d parameters, then XKT1, the count and %d settings (%ld bytes)", END_OF_SOUND_PARAMETERS, NUM_KIT_EXT, fileSize(argv[1], kitFile));
		memset(mainParameters[K], 0, END_OF_SOUND_PARAMETERS);
		memset(mainKitExt[K], 0x55, NUM_KIT_EXT);
		CHECK(preset_loadDrumsetSD(K, 0) == FR_OK, "loaded");
		CHECK(!memcmp(mainParameters[K], params, sizeof params) && !memcmp(mainKitExt[K], want, sizeof want), "parameters and settings as saved");

		preset_loadDrumsetRam(K, 0, 0);
		CHECK(!memcmp(&activeParameters[PAR_BEGINNING_OF_KIT_EXT], want, sizeof want), "loading the kit makes them the current settings");
		activeParameters[PAR_SCALE3] = SCALE_PELOG;
		activeParameters[PAR_VFX_SEND2] = 77;
		preset_saveDrumsetRam(K + 1, 0);
		CHECK(mainKitExt[K + 1][PAR_SCALE3 - PAR_BEGINNING_OF_KIT_EXT] == SCALE_PELOG && mainKitExt[K + 1][PAR_VFX_SEND2 - PAR_BEGINNING_OF_KIT_EXT] == 77,
			"saving the current kit to another slot takes them along");
		morphParameters = mainParameters[0];	/* as main.c sets it */
		preset_saveDrumsetRam(K + 2, 1);
		CHECK(!memcmp(mainKitExt[K + 2], mainKitExt[K + 1], NUM_KIT_EXT), "a morphed kit gets the current settings");

		writeKit(argv[1], kitFile, params, NULL, 0);
		memset(mainKitExt[K], 0x55, NUM_KIT_EXT);
		CHECK(preset_loadDrumsetSD(K, 0) == FR_OK && !memcmp(mainKitExt[K], defaults, NUM_KIT_EXT),
			"a kit file of the original firmware: the default settings");

		uint8_t tail[5 + 64];
		memcpy(tail, "XKT1", 4); tail[4] = NUM_KIT_EXT; memcpy(tail + 5, want, 10);
		writeKit(argv[1], kitFile, params, tail, 5 + 10);
		CHECK(preset_loadDrumsetSD(K, 0) == FR_OK && !memcmp(mainKitExt[K], defaults, NUM_KIT_EXT),
			"a cut off block: the default settings");

		memcpy(tail + 5, want, NUM_KIT_EXT);
		tail[5 + PAR_SCALE1 - PAR_BEGINNING_OF_KIT_EXT] = 9;
		tail[5 + PAR_VFX_TYPE4 - PAR_BEGINNING_OF_KIT_EXT] = 200;
		tail[5 + PAR_VFX_AMT6 - PAR_BEGINNING_OF_KIT_EXT] = 255;
		writeKit(argv[1], kitFile, params, tail, 5 + NUM_KIT_EXT);
		CHECK(preset_loadDrumsetSD(K, 0) == FR_OK && mainKitExt[K][PAR_SCALE1 - PAR_BEGINNING_OF_KIT_EXT] == SCALE_12_TONES
			&& mainKitExt[K][PAR_VFX_TYPE4 - PAR_BEGINNING_OF_KIT_EXT] == VFX_OFF && mainKitExt[K][PAR_VFX_AMT6 - PAR_BEGINNING_OF_KIT_EXT] == 127
			&& mainKitExt[K][PAR_VFX_SEND3 - PAR_BEGINNING_OF_KIT_EXT] == want[PAR_VFX_SEND3 - PAR_BEGINNING_OF_KIT_EXT],
			"values out of range are corrected, the others kept");

		memcpy(tail + 5, want, NUM_KIT_EXT);
		memset(tail + 5 + NUM_KIT_EXT, 99, 10);
		tail[4] = NUM_KIT_EXT + 10;
		writeKit(argv[1], kitFile, params, tail, 5 + NUM_KIT_EXT + 10);
		CHECK(preset_loadDrumsetSD(K, 0) == FR_OK && !memcmp(mainKitExt[K], want, NUM_KIT_EXT),
			"a longer block of a later version: its first %d settings", NUM_KIT_EXT);

		memcpy(&activeParameters[PAR_BEGINNING_OF_KIT_EXT], want, NUM_KIT_EXT);
		CHECK(preset_saveTmpKit() == FR_OK && fileSize(argv[1], "tmp.snd") == 1 + END_OF_SOUND_PARAMETERS + 5 + NUM_KIT_EXT, "tmp.snd saved (%ld bytes)", fileSize(argv[1], "tmp.snd"));
		memset(&activeParameters[PAR_BEGINNING_OF_KIT_EXT], 0, NUM_KIT_EXT);
		CHECK(preset_LoadTmpKit() == FR_OK && !memcmp(&activeParameters[PAR_BEGINNING_OF_KIT_EXT], want, NUM_KIT_EXT), "tmp.snd loaded with the settings");

		CHECK(preset_saveGlobals() == FR_OK && fileSize(argv[1], "glo.cfg") == END_OF_GLOBAL_PARAMETERS - PAR_BEGINNING_OF_GLOBALS,
			"glo.cfg keeps its %d globals (%ld bytes)", END_OF_GLOBAL_PARAMETERS - PAR_BEGINNING_OF_GLOBALS, fileSize(argv[1], "glo.cfg"));
	}
#endif

	printf("\n8) files to compare with the original firmware's\n");
	fill(PAT, 0);
	CHECK(preset_savePatternSD(PAT) == FR_OK, "pattern %d saved without extra locks", PAT);
	menu_playedPattern = PAT;
	CHECK(preset_saveTmpPat() == FR_OK, "tmp.pat saved");
#endif
	printf("\n%d of %d checks passed\n", checks - fails, checks);
	return fails ? 1 : 0;
}
