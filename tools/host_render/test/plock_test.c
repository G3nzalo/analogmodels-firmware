/*
 * plock_test.c - PC test of the step parameter locks: hold a step, turn
 * knobs, release, play the pattern, up to eight locks per step and a full
 * lock pool, and choosing an 808/909 instrument. Runs the real buttonHandler,
 * menu, CortexParser, sequencer and sound engine code, only the hardware is
 * stubbed. PC only, not part of the firmware.
 *
 * Part of a custom firmware for the Erica Synths LXR-02 (not an official
 * Erica Synths release). Added 2026.
 * SPDX-License-Identifier: GPL-3.0-only
 */
#include <stdio.h>
#include <string.h>
#include <xmmintrin.h>
#include <pmmintrin.h>
#include "buttonHandler.c"	/* the arm/disarm functions are static */
#include "Soundengine/DSPAudio/DrumVoice.h"
#include "Soundengine/DSPAudio/Snare.h"
#include "Soundengine/DSPAudio/CymbalVoice.h"
#include "Soundengine/DSPAudio/HiHat.h"
#include "Soundengine/DSPAudio/modulationNode.h"
#include "Soundengine/MIDI/ParameterArray.h"
#include "Soundengine/MIDI/MidiParser.h"
#include "Soundengine/Sequencer/sequencer.h"
#include "Preset/InitKit.h"
#include "Soundengine/DSPAudio/AnalogModels.h"
#include "Soundengine/DSPAudio/ResonantFilter.h"
#if __has_include("Soundengine/DSPAudio/VoiceSettings.h")
#include "Soundengine/DSPAudio/VoiceSettings.h"
#endif

extern uint8_t frontParser_activeTrack;
extern uint8_t seq_recordActive;
void test_setStepIndex(uint8_t track, int8_t step);
uint8_t test_autoNodeDest(uint8_t track, uint8_t slot);

static int fails = 0, checks = 0;
#define CHECK(cond, ...) do { checks++; if (cond) printf("  ok    "); else { printf("  FAIL  "); fails++; } \
	printf(__VA_ARGS__); printf("\n"); } while (0)

#define SNARE 3	/* track/voice 4 */
enum { SUB_OSC = 0, SUB_AEG = 1, SUB_FILTER = 5 };

static Step* st(uint8_t track, uint8_t s) { return &get_seq_patternSet_p()->seq_subStepPattern[seq_activePattern][track][s]; }
static uint8_t code(uint16_t par) { return (uint8_t)(par < 128 ? par + 1 : par); }	/* how a lock stores a parameter */
static int noLock(uint8_t nr) { return nr == NO_AUTOMATION || nr == 0; }
/* all locks of a step of the playing pattern (lock 1 and 2 and the extra ones) */
static int locks(uint8_t track, uint8_t s, uint8_t* nr, uint8_t* val) { return seq_getStepLocks(seq_activePattern, track, s, nr, val); }
static int numLocks(uint8_t track, uint8_t s) { uint8_t nr[NUM_STEP_LOCKS], val[NUM_STEP_LOCKS]; return locks(track, s, nr, val); }
/* the value of the step's lock of a parameter, -1 if it has none */
static int lockOf(uint8_t track, uint8_t s, uint16_t par)
{
	uint8_t nr[NUM_STEP_LOCKS], val[NUM_STEP_LOCKS];
	const int n = locks(track, s, nr, val);
	for (int k = 0; k < n; k++) if (nr[k] == code(par)) return val[k];
	return -1;
}

/* like loading a kit or turning a knob with no step held */
static void engineSet(uint16_t i, uint8_t val)
{
	MidiMsg msg = {0};
	activeParameters[i] = val;
	if (i < 128) { msg.status = MIDI_CC; msg.data1 = (uint8_t)((i + 1) & 0x7f); }
	else { msg.status = MIDI_CC2; msg.data1 = (uint8_t)((i - 128) & 0x7f); }
	msg.data2 = val;
	midiParser_ccHandler(msg, 1, 0, 0.f);
}
static float eng(uint16_t par) { return *(float*)parameterArray[par].ptr; }
static float engineFor(uint16_t par, uint8_t val)	/* engine value a parameter value gives */
{
	const uint8_t old = activeParameters[par];
	engineSet(par, val);
	const float f = eng(par);
	engineSet(par, old);
	return f;
}

/* front panel actions */
static void selectVoice(uint8_t v) { menu_switchPage(v); frontPanel_sendData(SEQ_CC, SEQ_SET_ACTIVE_TRACK, v); }
static void holdStep(uint8_t s) { buttonHandler_armTimerActionStep((int8_t)s); }
static void releaseStep(void) { buttonHandler_disarmTimerActionStep(); }
static void turn(uint8_t subPage, uint8_t pot, int16_t diff) { menu_switchSubPage(subPage, 0); menu_parseKnobValue(pot, diff); }
static void clickEncoder(void) { menu_parseEncoder(0, 1); menu_parseEncoder(0, 0); }

int main(void)
{
	_MM_SET_FLUSH_ZERO_MODE(_MM_FLUSH_ZERO_ON);
	_MM_SET_DENORMALS_ZERO_MODE(_MM_DENORMALS_ZERO_ON);
	static float nullTarget[4];
	parameterArray_init();
	for (int i = 0; i < END_OF_SOUND_PARAMETERS; i++)
		if (!parameterArray[i].ptr) parameterArray[i].ptr = nullTarget;
	for (int i = 0; i < 6; i++) modNode_init(&velocityModulators[i]);
	initDrumVoice(); Snare_init(); HiHat_init(); Cymbal_init();
	seq_init();
	menu_init();
	for (int i = 0; i < END_OF_SOUND_PARAMETERS; i++) engineSet((uint16_t)i, i < INIT_KIT_SIZE - 8 ? initKit[8 + i] : 0);
	/* the waveform as a model too, so the "chr" relabel path runs */
	engineSet(PAR_OSC_WAVE_SNARE, 8);
	/* a sound with room to move: filter half open, medium decay */
	engineSet(PAR_FILTER_FREQ_4, 60); engineSet(PAR_RESO_4, 20);
	engineSet(PAR_VELOD4, 70); engineSet(PAR_PWM_4, 40);

	const uint8_t baseF = activeParameters[PAR_FILTER_FREQ_4], baseQ = activeParameters[PAR_RESO_4];
	const uint8_t baseD = activeParameters[PAR_VELOD4], baseP = activeParameters[PAR_PWM_4];
	const float engF = eng(PAR_FILTER_FREQ_4), engD = eng(PAR_VELOD4), engP = eng(PAR_PWM_4);
	printf("snare base values: filter %d  reso %d  decay %d  chr/pwm %d (PAR_PWM_4 = %d, a parameter above 127)\n",
		baseF, baseQ, baseD, baseP, PAR_PWM_4);

	/* locks of other tracks and of another snare step that must survive */
	st(0, 5)->param1Nr = code(PAR_COARSE1); st(0, 5)->param1Val = 33;
	st(1, 6)->param2Nr = code(PAR_COARSE2); st(1, 6)->param2Val = 44;
	st(0, 1)->param1Nr = code(PAR_COARSE1); st(0, 1)->param1Val = 3;	/* same step number on the kick */
	st(SNARE, 9)->param1Nr = code(PAR_MIX1); st(SNARE, 9)->param1Val = 99;
	const uint8_t coarse1 = activeParameters[PAR_COARSE1];

	selectVoice(SNARE);
	CHECK(menu_getActiveVoice() == SNARE && frontParser_activeTrack == SNARE, "snare voice selected");

	printf("\n1) the example: step 1 normal, step 2 filter a bit open, step 3 other amp decay\n");
	menu_switchSubPage(SUB_FILTER, 0);
	holdStep(1);
	turn(SUB_FILTER, 0, +12);
	CHECK(st(SNARE, 1)->param1Nr == code(PAR_FILTER_FREQ_4) && st(SNARE, 1)->param1Val == baseF + 12,
		"step 2 got lock 1 = filter %d", st(SNARE, 1)->param1Val);
	CHECK(activeParameters[PAR_FILTER_FREQ_4] == baseF + 12, "the display shows the locked value while holding");
	CHECK(eng(PAR_FILTER_FREQ_4) == engF, "the sound itself is not changed while holding");
	releaseStep();
	CHECK(activeParameters[PAR_FILTER_FREQ_4] == baseF && eng(PAR_FILTER_FREQ_4) == engF, "release: display and sound back to the kit value");

	menu_switchSubPage(SUB_AEG, 0);
	holdStep(2);
	turn(SUB_AEG, 1, -20);
	CHECK(st(SNARE, 2)->param1Nr == code(PAR_VELOD4) && st(SNARE, 2)->param1Val == baseD - 20,
		"step 3 got lock 1 = amp decay %d", st(SNARE, 2)->param1Val);
	releaseStep();
	CHECK(activeParameters[PAR_VELOD4] == baseD && eng(PAR_VELOD4) == engD, "release: decay back to the kit value");
	CHECK(noLock(st(SNARE, 0)->param1Nr) && noLock(st(SNARE, 0)->param2Nr), "step 1 has no locks");

	printf("\n2) two locks on one step, then a third parameter (it used to replace lock 1)\n");
	seq_setActiveAutomationTrack(1);	/* as if REC had track 2 selected */
	holdStep(4);
	turn(SUB_FILTER, 0, +5);
	turn(SUB_FILTER, 1, +7);
	CHECK(st(SNARE, 4)->param1Nr == code(PAR_FILTER_FREQ_4) && st(SNARE, 4)->param2Nr == code(PAR_RESO_4),
		"step 5: lock 1 filter, lock 2 reso");
	turn(SUB_OSC, 5, +10);	/* chr/pwm, parameter number above 127 */
	CHECK(numLocks(SNARE, 4) == 3 && lockOf(SNARE, 4, PAR_PWM_4) == baseP + 10 &&
		st(SNARE, 4)->param1Nr == code(PAR_FILTER_FREQ_4) && st(SNARE, 4)->param1Val == baseF + 5 &&
		st(SNARE, 4)->param2Nr == code(PAR_RESO_4) && st(SNARE, 4)->param2Val == baseQ + 7,
		"the third parameter gets lock 3 (%d locks), lock 1 and 2 stay", numLocks(SNARE, 4));
	CHECK(st(0, 5)->param1Nr == code(PAR_COARSE1) && st(0, 5)->param1Val == 33, "kick lock on step 6 still there");
	CHECK(st(1, 6)->param2Nr == code(PAR_COARSE2) && st(1, 6)->param2Val == 44, "drum 2 lock on step 7 still there");
	CHECK(st(SNARE, 9)->param1Nr == code(PAR_MIX1) && st(SNARE, 9)->param1Val == 99, "snare lock on step 10 still there");
	CHECK(st(SNARE, 1)->param1Val == baseF + 12 && st(SNARE, 2)->param1Val == baseD - 20, "steps 2 and 3 untouched");
	releaseStep();
	CHECK(activeParameters[PAR_FILTER_FREQ_4] == baseF && activeParameters[PAR_RESO_4] == baseQ &&
		activeParameters[PAR_PWM_4] == baseP, "release: all three shown values back");
	CHECK(eng(PAR_FILTER_FREQ_4) == engF && eng(PAR_PWM_4) == engP, "release: sound back");
	CHECK(seq_getActiveAutomationTrack() == 1, "release: REC automation track back to track 2");
	seq_setActiveAutomationTrack(0);

	printf("\n3) holding a step again shows its locks (and not the kick's)\n");
	holdStep(1);
	CHECK(activeParameters[PAR_FILTER_FREQ_4] == baseF + 12, "step 2 shows filter %d", activeParameters[PAR_FILTER_FREQ_4]);
	CHECK(activeParameters[PAR_COARSE1] == coarse1, "kick coarse not touched by the kick's step 2 lock");
	turn(SUB_FILTER, 0, +2);
	CHECK(st(SNARE, 1)->param1Nr == code(PAR_FILTER_FREQ_4) && st(SNARE, 1)->param1Val == baseF + 14 &&
		noLock(st(SNARE, 1)->param2Nr), "turning it again edits the same lock (%d)", st(SNARE, 1)->param1Val);
	releaseStep();
	CHECK(activeParameters[PAR_FILTER_FREQ_4] == baseF, "release: back to the kit value");
	holdStep(4);
	CHECK(activeParameters[PAR_PWM_4] == baseP + 10 && activeParameters[PAR_RESO_4] == baseQ + 7,
		"step 5 shows chr %d and reso %d", activeParameters[PAR_PWM_4], activeParameters[PAR_RESO_4]);
	releaseStep();
	CHECK(activeParameters[PAR_PWM_4] == baseP && activeParameters[PAR_RESO_4] == baseQ, "release: back");

	printf("\n4) a step that only has lock 2\n");
	st(SNARE, 6)->param1Nr = NO_AUTOMATION;
	st(SNARE, 6)->param2Nr = code(PAR_FILTER_FREQ_4); st(SNARE, 6)->param2Val = 50;
	holdStep(6);
	turn(SUB_FILTER, 0, +3);
	CHECK(st(SNARE, 6)->param2Val == 53 && noLock(st(SNARE, 6)->param1Nr),
		"turning the locked knob edits lock 2 (lock1 %d/%d lock2 %d/%d)",
		st(SNARE, 6)->param1Nr, st(SNARE, 6)->param1Val, st(SNARE, 6)->param2Nr, st(SNARE, 6)->param2Val);
	releaseStep();
	CHECK(activeParameters[PAR_FILTER_FREQ_4] == baseF && eng(PAR_FILTER_FREQ_4) == engF, "release: back to the kit value");

	printf("\n4b) an empty knob slot does not use up a lock\n");
	holdStep(7);
	turn(SUB_AEG, 1, +5);	/* decay -> lock 1 */
	turn(SUB_AEG, 5, +4);	/* no parameter on this knob */
	turn(SUB_AEG, 0, +3);	/* attack -> lock 2 */
	CHECK(st(SNARE, 7)->param1Nr == code(PAR_VELOD4) && st(SNARE, 7)->param2Nr == code(PAR_VELOA4),
		"step 8: lock 1 decay, lock 2 attack (%d/%d)", st(SNARE, 7)->param1Nr, st(SNARE, 7)->param2Nr);
	releaseStep();
	CHECK(activeParameters[PAR_VELOD4] == baseD, "release: decay back");
	st(SNARE, 7)->param1Nr = st(SNARE, 7)->param2Nr = NO_AUTOMATION;

	printf("\n5) encoder click while holding clears the step\n");
	holdStep(4);
	clickEncoder();
	CHECK(noLock(st(SNARE, 4)->param1Nr) && noLock(st(SNARE, 4)->param2Nr) && numLocks(SNARE, 4) == 0, "step 5 locks cleared (all three)");
	CHECK(activeParameters[PAR_PWM_4] == baseP && activeParameters[PAR_RESO_4] == baseQ, "shown values back to the kit");
	releaseStep();
	CHECK(activeParameters[PAR_PWM_4] == baseP && activeParameters[PAR_RESO_4] == baseQ && eng(PAR_PWM_4) == engP,
		"release after clearing: still the kit values");
	CHECK(st(SNARE, 1)->param1Val == baseF + 14 && st(SNARE, 9)->param1Val == 99 && st(0, 5)->param1Val == 33,
		"other locks untouched");

	printf("\n6) another sound type on step 4 (waveform lock: Snr -> Clp)\n");
	const uint8_t baseW = activeParameters[PAR_OSC_WAVE_SNARE];
	holdStep(3);
	turn(SUB_OSC, 4, +1);
	CHECK(st(SNARE, 3)->param1Nr == code(PAR_OSC_WAVE_SNARE) && st(SNARE, 3)->param1Val == baseW + 1,
		"step 4 got lock 1 = waveform %d", st(SNARE, 3)->param1Val);
	CHECK(snareVoice.osc.waveform == baseW, "the sound keeps its waveform while holding");
	releaseStep();
	CHECK(activeParameters[PAR_OSC_WAVE_SNARE] == baseW && snareVoice.osc.waveform == baseW, "release: waveform back");

	printf("\n7) playback\n");
	const float lockedF = engineFor(PAR_FILTER_FREQ_4, (uint8_t)(baseF + 14));
	const float lockedD = engineFor(PAR_VELOD4, (uint8_t)(baseD - 20));
	for (int round = 0; round < 2; round++) {
		for (int s = 0; s < 6; s++) {
			test_setStepIndex(SNARE, (int8_t)s);
			seq_triggerVoice(SNARE, 100, 60);
			const float f = eng(PAR_FILTER_FREQ_4), d = eng(PAR_VELOD4);
			const uint8_t w = snareVoice.osc.waveform;
			const float wantF = (s == 1) ? lockedF : engF, wantD = (s == 2) ? lockedD : engD;
			const uint8_t wantW = (s == 3) ? baseW + 1 : baseW;
			CHECK(f == wantF && d == wantD && w == wantW, "pass %d step %d: filter %s, decay %s, waveform %d", round + 1, s + 1,
				f == engF ? "kit" : (f == lockedF ? "locked" : "??"), d == engD ? "kit" : (d == lockedD ? "locked" : "??"), w);
		}
	}

	printf("\n8) choosing an 808/909 instrument sets up the voice\n");
	selectVoice(0);
	engineSet(PAR_OSC_WAVE_DRUM1, 0);
	engineSet(PAR_VELOD1, 100); engineSet(PAR_FILTER_TYPE_1, 0); engineSet(PAR_MODAMNT1, 90);
	engineSet(PAR_PWM_1, 50); engineSet(PAR_COARSE1, 30); engineSet(PAR_DRIVE1, 40);
	menu_switchSubPage(SUB_OSC, 0);
	turn(SUB_OSC, 2, MODEL_BD9);	/* waveform knob from Sin to BD9 */
	CHECK(activeParameters[PAR_OSC_WAVE_DRUM1] == MODEL_BD9 && voiceArray[0].osc.waveform == MODEL_BD9, "drum 1 plays BD9");
	CHECK(activeParameters[PAR_COARSE1] == 60 && activeParameters[PAR_VELOD1] == 47 && activeParameters[PAR_VOL_SLOPE1] == 127,
		"tuning 0, its decay and the gate envelope (coa %d dec %d slope %d)",
		activeParameters[PAR_COARSE1], activeParameters[PAR_VELOD1], activeParameters[PAR_VOL_SLOPE1]);
	CHECK(activeParameters[PAR_FILTER_TYPE_1] == 7 && voiceArray[0].filterType == FILTER_OFF, "filter off in the menu and the engine");
	CHECK(activeParameters[PAR_MODAMNT1] == 0 && voiceArray[0].egPitchModAmount == 0.f, "pitch EG off in the menu and the engine");
	CHECK(activeParameters[PAR_PWM_1] == 0 && activeParameters[PAR_DRIVE1] == 0, "saturator and drive off");
	holdStep(2);
	turn(SUB_OSC, 2, +1);	/* BD9 -> SD8 on one step only */
	CHECK(st(0, 2)->param1Nr == code(PAR_OSC_WAVE_DRUM1) && st(0, 2)->param1Val == MODEL_SD8, "a held step gets a waveform lock");
	CHECK(activeParameters[PAR_VELOD1] == 47 && voiceArray[0].osc.waveform == MODEL_BD9, "but no preset: the sound keeps its settings");
	releaseStep();
	CHECK(activeParameters[PAR_OSC_WAVE_DRUM1] == MODEL_BD9, "release: back to BD9");
	seq_recordActive = 1;
	engineSet(PAR_VELOD1, 90);
	turn(SUB_OSC, 2, -1);	/* BD9 -> BD8 while recording */
	CHECK(activeParameters[PAR_OSC_WAVE_DRUM1] == MODEL_BD8 && activeParameters[PAR_VELOD1] == 90, "no preset while recording automation");
	seq_recordActive = 0;
	turn(SUB_OSC, 2, +3);	/* BD8 -> SD9 */
	CHECK(activeParameters[PAR_VELOD1] == 32 && activeParameters[PAR_COARSE1] == 60, "SD9 brings its own decay (%d)", activeParameters[PAR_VELOD1]);
	selectVoice(5);
	turn(SUB_OSC, 2, 0);
	menu_switchSubPage(SUB_OSC, 0);
	engineSet(PAR_WAVE1_HH, 0);
	turn(SUB_OSC, 2, MODEL_HH8);
	CHECK(activeParameters[PAR_VELOD6_CLOSED] == 7 && activeParameters[PAR_VELOD6_OPEN] == 44, "hi-hat voice: closed and open decay for HH8 (%d/%d)",
		activeParameters[PAR_VELOD6_CLOSED], activeParameters[PAR_VELOD6_OPEN]);
	selectVoice(SNARE);

	printf("\n9) eight locks on one step, a ninth parameter replaces the oldest\n");
	{
		/* ten snare knobs: osc page coarse, fine, noise freq, mix; amp EG attack,
		 * decay; filter freq, reso, drive; pitch EG amount */
		const struct { uint8_t sub, pot; } knob[10] = { {0,0},{0,1},{0,2},{0,3},{1,0},{1,1},{5,0},{5,1},{5,3},{2,2} };
		uint16_t par[10]; uint8_t base[10], lockedVal[10]; int8_t dir[10]; float engBase[10];
		int isFlt[10];
		for (int k = 0; k < 10; k++) {
			menu_switchSubPage(knob[k].sub, 0);
			par[k] = pgm_read_word(&menuPages[menu_activePage][knob[k].sub].bot1 + knob[k].pot);
			base[k] = activeParameters[par[k]];
			dir[k] = base[k] > 64 ? -4 : 4;
			isFlt[k] = parameterArray[par[k]].type == TYPE_FLT;
			engBase[k] = isFlt[k] ? eng(par[k]) : 0.f;
		}
		printf("        parameters:");
		for (int k = 0; k < 10; k++) printf(" %d", par[k]);
		printf("\n");
		const uint8_t S = 12, poolBefore = seq_getLockPool(seq_activePattern)->count;
		CHECK(numLocks(SNARE, S) == 0, "step 13 starts without locks");
		holdStep(S);
		for (int k = 0; k < 8; k++) {
			turn(knob[k].sub, knob[k].pot, dir[k]);
			lockedVal[k] = activeParameters[par[k]];
		}
		int all = numLocks(SNARE, S) == 8;
		for (int k = 0; k < 8; k++) all = all && lockOf(SNARE, S, par[k]) == lockedVal[k] && lockedVal[k] != base[k];
		CHECK(all, "step 13 has 8 locks, one per turned knob (%d)", numLocks(SNARE, S));
		CHECK(seq_getLockPool(seq_activePattern)->count == poolBefore + 6, "lock 3 to 8 come from the pattern's pool (%d)",
			seq_getLockPool(seq_activePattern)->count - poolBefore);
		CHECK(eng(PAR_VELOD4) == engD && eng(PAR_FILTER_FREQ_4) == engF, "the sound itself is not changed while holding");
		turn(knob[3].sub, knob[3].pot, dir[3]);
		lockedVal[3] = activeParameters[par[3]];
		CHECK(numLocks(SNARE, S) == 8 && lockOf(SNARE, S, par[3]) == lockedVal[3] && lockOf(SNARE, S, par[0]) == lockedVal[0],
			"turning a locked knob again edits its lock, nothing is replaced");
		turn(knob[8].sub, knob[8].pot, dir[8]);
		lockedVal[8] = activeParameters[par[8]];
		CHECK(numLocks(SNARE, S) == 8 && lockOf(SNARE, S, par[8]) == lockedVal[8] && lockOf(SNARE, S, par[0]) == -1,
			"a ninth parameter replaces the first lock");
		CHECK(activeParameters[par[0]] == base[0], "the display shows the sound's value of the replaced parameter again");
		turn(knob[9].sub, knob[9].pot, dir[9]);
		lockedVal[9] = activeParameters[par[9]];
		CHECK(numLocks(SNARE, S) == 8 && lockOf(SNARE, S, par[9]) == lockedVal[9] && lockOf(SNARE, S, par[1]) == -1 &&
			lockOf(SNARE, S, par[8]) == lockedVal[8], "a tenth replaces the second one");
		releaseStep();
		all = 1;
		for (int k = 0; k < 10; k++) all = all && activeParameters[par[k]] == base[k] && (!isFlt[k] || eng(par[k]) == engBase[k]);
		CHECK(all, "release: all ten shown values and the sound back to the kit");
		holdStep(S);
		all = 1;
		for (int k = 2; k < 10; k++) all = all && activeParameters[par[k]] == lockedVal[k];
		CHECK(all && activeParameters[par[0]] == base[0], "holding it again shows the 8 locked values");
		releaseStep();

		/* playback: the 8 locks, then a step without locks */
		float engLocked[10];
		for (int k = 2; k < 10; k++) engLocked[k] = isFlt[k] ? engineFor(par[k], lockedVal[k]) : 0.f;
		test_setStepIndex(SNARE, (int8_t)S);
		seq_triggerVoice(SNARE, 100, 60);
		all = 1;
		for (int k = 2; k < 10; k++) all = all && (!isFlt[k] || eng(par[k]) == engLocked[k]);
		for (int k = 0; k < 2; k++) all = all && (!isFlt[k] || eng(par[k]) == engBase[k]);
		int dests = 0;
		for (int slot = 0; slot < NUM_STEP_LOCKS; slot++)
			for (int k = 2; k < 10; k++) if (test_autoNodeDest(SNARE, (uint8_t)slot) == code(par[k])) dests++;
		CHECK(all && dests == 8, "step 13 plays all 8 locks (%d lock nodes)", dests);
		test_setStepIndex(SNARE, (int8_t)(S + 1));
		seq_triggerVoice(SNARE, 100, 60);
		all = 1;
		for (int k = 0; k < 10; k++) all = all && (!isFlt[k] || eng(par[k]) == engBase[k]);
		for (int slot = 0; slot < NUM_STEP_LOCKS; slot++) all = all && test_autoNodeDest(SNARE, (uint8_t)slot) == NO_AUTOMATION;
		CHECK(all, "step 14 (no locks): the sound is back to the kit");
		test_setStepIndex(SNARE, (int8_t)S);
		seq_triggerVoice(SNARE, 100, 60);
		test_setStepIndex(SNARE, 1);
		seq_triggerVoice(SNARE, 100, 60);
		CHECK(eng(PAR_FILTER_FREQ_4) == lockedF && eng(PAR_VELOD4) == engD && (!isFlt[3] || eng(par[3]) == engBase[3]),
			"from step 13 to step 2: only step 2's filter lock is left");
		test_setStepIndex(SNARE, 0);
		seq_triggerVoice(SNARE, 100, 60);

		printf("\n10) the lock pool of the pattern is full\n");
		uint8_t fillers = 0;
		for (int st2 = 0; st2 < NUM_STEPS && seq_getLockPool(seq_activePattern)->count < LOCK_POOL_SIZE; st2++)
			for (int k = 0; k < NUM_STEP_LOCKS && seq_getLockPool(seq_activePattern)->count < LOCK_POOL_SIZE; k++)
				if (seq_setStepLock(seq_activePattern, 2, (uint8_t)st2, (uint8_t)(code(PAR_COARSE3) + k), 1)) fillers++;
		CHECK(seq_getLockPool(seq_activePattern)->count == LOCK_POOL_SIZE, "pool full (%d locks on drum 3)", fillers);
		holdStep(14);
		turn(SUB_FILTER, 0, +3);
		turn(SUB_FILTER, 1, +3);
		CHECK(numLocks(SNARE, 14) == 2, "step 15 still gets lock 1 and 2");
		turn(SUB_AEG, 1, -3);
		CHECK(numLocks(SNARE, 14) == 2 && lockOf(SNARE, 14, PAR_VELOD4) == baseD - 3 && lockOf(SNARE, 14, PAR_FILTER_FREQ_4) == -1 &&
			lockOf(SNARE, 14, PAR_RESO_4) == baseQ + 3, "a third parameter replaces lock 1 (no room in the pool)");
		releaseStep();
		CHECK(activeParameters[PAR_FILTER_FREQ_4] == baseF && activeParameters[PAR_VELOD4] == baseD, "release: back to the kit");
		seq_removeExtraLocks(seq_activePattern, 2, 0xff);
		for (int st2 = 0; st2 < NUM_STEPS; st2++) seq_clearStepLocks(2, seq_activePattern, (uint8_t)st2);
		CHECK(seq_getLockPool(seq_activePattern)->count == poolBefore + 6, "drum 3's locks removed again");

		printf("\n11) recording automation while playing\n");
		const uint8_t R = 20;
		seq_setMainStep(seq_activePattern, SNARE, R, 1);
		test_setStepIndex(SNARE, (int8_t)R);
		seq_setActiveAutomationTrack(0);
		seq_recordActive = 1;
		turn(SUB_FILTER, 0, +6);
		turn(SUB_FILTER, 1, +6);
		turn(SUB_AEG, 1, -6);
		turn(SUB_FILTER, 0, +2);
		seq_recordActive = 0;
		CHECK(numLocks(SNARE, R) == 3 && lockOf(SNARE, R, PAR_FILTER_FREQ_4) == baseF + 8 && lockOf(SNARE, R, PAR_RESO_4) == baseQ + 6 &&
			lockOf(SNARE, R, PAR_VELOD4) == baseD - 6, "three knobs recorded into step 21: three locks, the filter updated");
		engineSet(PAR_FILTER_FREQ_4, baseF); engineSet(PAR_RESO_4, baseQ); engineSet(PAR_VELOD4, baseD);
		seq_clearStepLocks(SNARE, seq_activePattern, R);

		printf("\n12) a knob without a parameter and CC 0 record nothing\n");
		seq_recordActive = 1;
		turn(SUB_AEG, 5, +4);	/* no parameter on this knob */
		CHECK(numLocks(SNARE, R) == 0, "an empty knob slot while recording: no lock on step 21 (%d)", numLocks(SNARE, R));
		seq_clearStepLocks(SNARE, seq_activePattern, R);
		seq_setStepLock(seq_activePattern, SNARE, R, code(PAR_FILTER_FREQ_4), 50);	/* lock 1 */
		seq_setActiveAutomationTrack(0);
		seq_recordAutomation(SNARE, 0, 77);
		seq_recordAutomation(SNARE, NO_AUTOMATION, 77);
		CHECK(numLocks(SNARE, R) == 1 && lockOf(SNARE, R, PAR_FILTER_FREQ_4) == 50,
			"parameter 0 and NO_AUTOMATION are not recorded: the step keeps its lock (%d)", numLocks(SNARE, R));
		seq_clearStepLocks(SNARE, seq_activePattern, R);
		seq_recordActive = 0;
		engineSet(PAR_BANK_CHANGE, 5);	/* sent as CC 0, like a kit load does */
		CHECK(midiParser_originalCcValues[0] == 5, "CC 0 stays inside the CC value table (%d)", midiParser_originalCcValues[0]);
		seq_setMainStep(seq_activePattern, SNARE, R, 0);
	}

#ifdef NUM_KIT_EXT
	printf("\n13) the scale and the voice FX of a voice (kit extension)\n");
	selectVoice(0);
	activeParameters[PAR_SCALE1] = 0;
	turn(SUB_OSC, 4, +1);	/* knob 5 of the oscillator page */
	CHECK(activeParameters[PAR_SCALE1] == SCALE_SLENDRO && voiceArray[0].osc.scale == SCALE_SLENDRO, "scl: slendro in the menu and the engine");
	turn(SUB_OSC, 4, +5);
	CHECK(activeParameters[PAR_SCALE1] == SCALE_PELOG && voiceArray[0].osc.scale == SCALE_PELOG, "pelog is the last one");
	holdStep(3);
	turn(SUB_OSC, 4, -2);
	CHECK(numLocks(0, 3) == 0 && voiceArray[0].osc.scale == SCALE_12_TONES, "a held step: no lock, the scale changes for the kit");
	turn(SUB_FILTER, 4, +2);	/* knob 5 of the filter page */
	turn(SUB_FILTER, 5, +40);
	turn(SUB_FILTER, 7, +64);
	CHECK(numLocks(0, 3) == 0 && activeParameters[PAR_VFX_TYPE1] == VFX_TUBE && activeParameters[PAR_VFX_AMT1] == 40
		&& activeParameters[PAR_VFX_SEND1] == 64, "voice FX tub, amount 40, send 64: no locks either");
	releaseStep();
	turn(SUB_FILTER, 4, +10);
	CHECK(activeParameters[PAR_VFX_TYPE1] == VFX_RING, "rng is the last voice FX");
	menu_switchSubPage(SUB_OSC, 0);
	turn(SUB_OSC, 2, MODEL_GNG - activeParameters[PAR_OSC_WAVE_DRUM1]);
	CHECK(voiceArray[0].osc.waveform == MODEL_GNG && activeParameters[PAR_VELOD1] == 120 && activeParameters[PAR_COARSE1] == 60,
		"the gong ageng brings its voice settings too (dec %d)", activeParameters[PAR_VELOD1]);
	turn(SUB_OSC, 2, +100);
	CHECK(activeParameters[PAR_OSC_WAVE_DRUM1] == MODEL_END - 1, "the waveform list ends with TKR (%d)", activeParameters[PAR_OSC_WAVE_DRUM1]);
#endif

	printf("\n%d of %d checks passed\n", checks - fails, checks);
	return fails ? 1 : 0;
}
