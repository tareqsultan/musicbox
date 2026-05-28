#include <Wire.h>
#include <FastLED.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <LedControl.h> 
#include <EEPROM.h>
#include <SD.h> 

// تشغيل محرك ومكتبات الصوت الصلبة العتادية للـ Teensy (MQS)
#define FASTLED_NO_FFT
#include <Audio.h>
#include <SPI.h>
#include <SerialFlash.h>
#include "Config.h"


int barClockCounter = 0;      // عداد الخطوات لحساب الموازير
int barTargetCount = 8;       // الهدف: التغيير كل 8 موازير كاملة
// ================================================================
// --- نماذج الدوال (Prototypes) لضمان التعرف عليها في الـ Scope ---
// ================================================================
int getMatrixXY(int x, int y);
void renderLedsOutput();
void updateOLED();
void handleKnobs(unsigned long currentMillis);
void handleEncoder();
void handleInterfaceButtons(unsigned long currentMillis);
void runSequencerEngine();
void runPhysicsEngine(float sA, float sB);
void midiSend(byte c, byte d1, byte d2);
void allNotesOff();
void updateWallMap();
void updateMatrixDisplay(int n);
void savePresetToSD(int presetNum);
void loadPresetFromSD(int presetNum);
void sendInstrumentChange(byte channel, byte patchNum);
void drawLetter(char l, int sX, int sY, CRGB c, CRGB* tL);
void onClock();
void onStart();
void onStop();
void onContinue();

// 🌟 البناء الفعلي لكائنات الصوت في الملف الرئيسي
AudioPlaySdWav          playWav[NUM_SAMPLE_LAYERS]; 
AudioPlaySdWav          loopPlayer;       
AudioFilterStateVariable lowPassFilter;    
AudioMixer4              mixerLeft1;  
AudioMixer4              mixerLeft2;  
AudioMixer4              mixerRight1; 
AudioMixer4              mixerRight2; 
AudioMixer4              finalMixLeft;
AudioMixer4              finalMixRight;
AudioOutputMQS          mqsAudioOut;


AudioAnalyzePeak         loopPeakAnalyzer;

AudioConnection          patchLoopToAnalyzer(loopPlayer, 0, loopPeakAnalyzer, 0);
// 🌟 البناء الفعلي لروابط الكابلات الموسيقية الصريحة
AudioConnection          patchL0(playWav[0], 0, mixerLeft1, 0);
AudioConnection          patchL1(playWav[1], 0, mixerLeft1, 1);
AudioConnection          patchL2(playWav[2], 0, mixerLeft1, 2);
AudioConnection          patchL3(playWav[3], 0, mixerLeft1, 3);
AudioConnection          patchL4(playWav[4], 0, mixerLeft2, 0);
AudioConnection          patchL5(playWav[5], 0, mixerLeft2, 1);
AudioConnection          patchL6(playWav[6], 0, mixerLeft2, 2);
AudioConnection          patchL7(playWav[7], 0, mixerLeft2, 3);

AudioConnection          patchR0(playWav[0], 1, mixerRight1, 0);
AudioConnection          patchR1(playWav[1], 1, mixerRight1, 1);
AudioConnection          patchR2(playWav[2], 1, mixerRight1, 2);
AudioConnection          patchR3(playWav[3], 1, mixerRight1, 3);
AudioConnection          patchR4(playWav[4], 1, mixerRight2, 0);
AudioConnection          patchR5(playWav[5], 1, mixerRight2, 1);
AudioConnection          patchR6(playWav[6], 1, mixerRight2, 2);
AudioConnection          patchR7(playWav[7], 1, mixerRight2, 3);

AudioConnection          mergeL1(mixerLeft1, 0, finalMixLeft, 0);
AudioConnection          mergeL2(mixerLeft2, 0, finalMixLeft, 1);
AudioConnection          mergeR1(mixerRight1, 0, finalMixRight, 0);
AudioConnection          mergeR2(mixerRight2, 0, finalMixRight, 1);

AudioConnection          patchLoopToFilter(loopPlayer, 0, lowPassFilter, 0);
AudioConnection          patchFilterToFinalL(lowPassFilter, 0, finalMixLeft, 2);
AudioConnection          patchFilterToFinalR(lowPassFilter, 0, finalMixRight, 2);

AudioConnection          patchOutL(finalMixLeft, 0, mqsAudioOut, 0); 
AudioConnection          patchOutR(finalMixRight, 0, mqsAudioOut, 1);

const char* sampleBank[8] = { "0.wav", "1.wav", "2.wav", "3.wav", "4.wav", "5.wav", "6.wav", "7.wav" };
const char* loopSampleBank[4] = { "LOOP0.WAV", "LOOP1.WAV", "LOOP2.WAV", "LOOP3.WAV" }; 

// ================================================================
// --- الهيكل والمعمارية العامة للنظام الموحد بعد التوسيع ---
// ================================================================
enum AppMode { MAIN_MENU, MELODIC_MODE, RHYTHM_MODE, PHYSICS_MODE, SAMPLER_MODE, LOOPER_MODE };
AppMode currentMode = MAIN_MENU;

CRGB ledsM1[NUM_LEDS];
CRGB ledsM2[NUM_LEDS]; 
Adafruit_SSD1306 oled(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, -1);
LedControl matrixDisplay = LedControl(29, 30, 31, 1);

bool gridRB[8][32] = {false};
uint8_t velocityGrid[8][32];
uint8_t octaveGrid[8][32]; 
bool sdCardReady = false;
unsigned long lastPress = 0;

bool externalSync = false;
volatile uint32_t clockTicks = 0;
bool dawRunning = false;
unsigned long lastClockMillis = 0;
int testPlayhead = 0; 
unsigned long lastStepTime = 0;
unsigned long stepInterval = 135; 
bool isPlaying = false;
int totalSteps = 32; 
unsigned long lastAnalogReadTime = 0;

CRGB colors[4]; 
int lastPlayStopState = HIGH; int lastSwState = HIGH; int lastClk;
unsigned long swPressedTime = 0; 
int currentPreset = 1;
bool matrixBlinkActive = false; unsigned long matrixBlinkTimer = 0;

// --- متغيرات خاصة بالوضع الثالث (المحرك الفيزيائي للأضلاع الثمانية الأصيلة) ---
bool wallMap[32][16] = {false};
struct Wall { int x, y, length; };
Wall walls[8] = { {23, 0, 0}, {23, 15, 0}, {16, 7, 0}, {31, 7, 0}, {7, 0, 0}, {7, 15, 0}, {0, 7, 0}, {15, 7, 0} };
int currentSide = 0; int cursorX = 0, cursorY = 0; int editModePhysics = 0; 
bool lastBallAState = true; bool lastBallBState = true;
bool ballAActive = true;    bool ballBActive = true;




#define NUM_PLUCKED_INST 13  // 🌟 تم زيادة العدد من 12 إلى 13 لاستيعاب البيانو

// 🌟 إضافة آلة البيانو (رقم 0 في الـ MIDI) في بداية المصفوفة
const byte pluckedInstruments[NUM_PLUCKED_INST] = {0, 12, 13, 24, 25, 45, 46, 104, 105, 106, 107, 108, 114};

// 🌟 إضافة الاسم المختصر للبيانو "Piano" في بداية مصفوفة الأسماء
const char* pluckedNames[NUM_PLUCKED_INST] = { 
    "Piano", "Marim", "Xylo", "Gtr.N", "Gtr.S", "Pizz", "Harp", 
    "Sitar", "Banjo", "Shami", "Koto", "Kalim", "Steel" 
};
int pluckedIndexA = 6; int pluckedIndexB = 6; 
/*
#define NUM_CHORDS 4
#define CHORD_SIZE 12
const int chords[NUM_CHORDS][CHORD_SIZE] = {
    {-12, -8, -5, -1, 0, 4, 7, 11, 12, 16, 19, 23},  
    {-12, -9, -5, -2, 0, 3, 7, 10, 12, 15, 19, 22},  
    {-12, -8, -5, -2, 0, 4, 7, 10, 12, 16, 19, 22},  
    {-12, -7, -5, 0,  0, 5, 7, 12, 12, 17, 19, 24}   
};
// تعريف مصفوفة أسماء الكوردات (5 عناصر)
const char* chordNames[5] = {"Maj7", "Min7", "Dom7", "Sus4", "RND"};

// تعريف متغير المؤشر (ابدأه بـ 0)
int currentChordIndex = 0; 
int rootNotePhysics = 60; int octaveA = 0; int octaveB = 0;
*/
#define NUM_CHORDS 4
#define CHORD_SIZE 12

const int chords[NUM_CHORDS][CHORD_SIZE] = {
    {-12, -8, -5, -1, 0, 4, 7, 11, 12, 16, 19, 23},  
    {-12, -9, -5, -2, 0, 3, 7, 10, 12, 15, 19, 22},  
    {-12, -8, -5, -2, 0, 4, 7, 10, 12, 16, 19, 22},  
    {-12, -7, -5, 0,  0, 5, 7, 12, 12, 17, 19, 24}   
};

// هنا الأسماء 4 فقط، وبدون RND
const char* chordNames[NUM_CHORDS] = {"Maj7", "Min7", "Dom7", "Sus4"};

// المتغيرات العامة
int currentChordIndex = 0; 
bool isChordRandom = false; // مفتاح العشوائية المنفصل
int rootNotePhysics = 60; int octaveA = 0; int octaveB = 0;

#define POLY_SIZE 4
struct MidiNote { byte note; unsigned long offTime; bool active; };
MidiNote polyA[POLY_SIZE]; MidiNote polyB[POLY_SIZE];
int globalNoteLengthMs = 200; 

float ballAX = 23.0, ballAY = 7.0, velAX = 0.6, velAY = 0.4;
float ballBX = 7.0, ballBY = 7.0, velBX = 0.5, velBY = 0.3;

// --- متغيرات خاصة بالوضع الخامس المطور (MIDI SYNC LOOPER) ---
int activeLoopIndex = 0;     
int loopCutoffHz = 4000;
int looperMaxSteps = 32; 
int rawKnobLengthValue = 1023; 
unsigned long looperTimer = 0; 
uint32_t looperWindowMs = 2000;    

// --- متغيرات خاصة بالوضع الأول والثاني والرابع (Sequencer & Sampler) ---
int currentInstrument = 0; int currentScale = 0; int rootNoteMelodic = 36;
int playModeMelodic = 0; int melodicChannel = 0; int noteLengthPercent = 80;
bool noteIsActiveInStep = false; int activeNotePlaying = 0; int melodicMenuIndex = 0; 
const char* scaleNames[] = {"MAJOR", "MINOR", "DORIAN", "MYXO", "BLUES"};
const char* modeNames[] = {"FORWARD", "BACKWARD", "RANDOM", "CHANCE"};
int layerNotes[8] = {36, 38, 42, 46, 39, 41, 45, 49}; 
int rhythmChannel = 9; int activeRhythmLayer = 0; int oledActiveLine = 0;
bool lastRhythmNotes[8] = {false}; int menuSelection = 0;
int lastLengthKnobValue = -999, lastBPMKnobValue = -999, lastVelocityKnobValue = -999;

// ================================================================
// --- الدوال المساعدة ودوال الرسم الموحدة هندسياً ---
// ================================================================
int getMatrixXY(int x, int y) { 
    int pX = 31 - x; int pY = 7 - y; 
    return (pX % 2 == 0) ? (pX * 8) + pY : (pX * 8) + (7 - pY); 
}

void setUnifiedPixelPhysics(int x, int y, CRGB color) {
    if (y < 0 || y > 15 || x < 0 || x > 31) return;
    if (y < 8) {
        int idx = getMatrixXY(x, y);
        if (idx >= 0 && idx < NUM_LEDS) ledsM2[idx] += color;
    } else {
        int y_offset = y - 8;
        int idx = getMatrixXY(x, y_offset);
        if (idx >= 0 && idx < NUM_LEDS) ledsM1[idx] += color;
    }
}

void updateMatrixDisplay(int n) { 
    matrixDisplay.clearDisplay(0); 
    int t=(n/10)%10, o=n%10; 
    for(int r=0; r<5; r++) matrixDisplay.setRow(0,r+1,(matrixDigits[t][r]<<5)|(matrixDigits[o][r]<<1)); 
}


void updateWallMap() {
    for(int x=0; x<32; x++) for(int y=0; y<16; y++) wallMap[x][y] = false; 
    for(int s=0; s<8; s++) {
        int dx = (s==2 || s==6) ? 1 : (s==3 || s==7) ? -1 : 0;
        int dy = (s==0 || s==4) ? 1 : (s==1 || s==5) ? -1 : 0;
        for(int i=0; i < walls[s].length; i++) {
            int nx = walls[s].x + (dx * i); int ny = walls[s].y + (dy * i);
            if(nx >= 0 && nx <= 31 && ny >= 0 && ny <= 15) wallMap[nx][ny] = true;
        }
    }
}



// ================================================================
// --- محرّكات التشغيل وإدارة الإشارات (Engines) ---
// ================================================================
void runSequencerEngine() {
    if (currentMode == MELODIC_MODE) {
        if (noteIsActiveInStep) { midiSend(0x80|melodicChannel, activeNotePlaying, 0); noteIsActiveInStep = false; }
        for (int y=0; y<8; y++) if (gridRB[y][testPlayhead]) {
            int scaleDegree = 7 - y; int off = (octaveGrid[y][testPlayhead]-1)*12; 
            activeNotePlaying = rootNoteMelodic + scaleIntervals[currentScale][scaleDegree] + off;
            midiSend(0x90|melodicChannel, activeNotePlaying, map(velocityGrid[y][testPlayhead], 0, 7, 15, 127)); 
            noteIsActiveInStep = true; break; 
        }
    } 
    else if (currentMode == RHYTHM_MODE) {
        for (int i=0; i<8; i++) if (lastRhythmNotes[i]) { midiSend(0x80|rhythmChannel, layerNotes[i], 0); lastRhythmNotes[i] = false; }
        for (int y=0; y<8; y++) if (gridRB[y][testPlayhead]) { midiSend(0x90|rhythmChannel, layerNotes[y], map(velocityGrid[y][testPlayhead], 0, 7, 15, 127)); lastRhythmNotes[y] = true; }
    }
    else if (currentMode == RHYTHM_MODE) {
        for (int i=0; i<8; i++) if (lastRhythmNotes[i]) { midiSend(0x80|rhythmChannel, layerNotes[i], 0); lastRhythmNotes[i] = false; }
        for (int y=0; y<8; y++) if (gridRB[y][testPlayhead]) { midiSend(0x90|rhythmChannel, layerNotes[y], map(velocityGrid[y][testPlayhead], 0, 7, 15, 127)); lastRhythmNotes[y] = true; }
    }
    else if (currentMode == SAMPLER_MODE) {
        for (int y = 0; y < 8; y++) { 
            if (gridRB[y][testPlayhead]) { 
                int sIdx = samplerNotes[y]; 
                
                if(sIdx >= 0 && sIdx <= 20) {
                    // 🌟 1. قراءة قيمة الفلوسيتي للخطوة الحالية (من 0 إلى 7)
                    int stepVelocity = velocityGrid[y][testPlayhead];
                    
                    // 🌟 2. تحويل القراءة (0-7) إلى قيمة Gain للميكسر (من 0.0 صمت إلى 1.0 أعلى صوت)
                    float dynamicGain = map(stepVelocity, 0, 7, 0, 100) / 100.0;
                    
                    // 🌟 3. تحديث الميكسرات الصوتية (Left & Right) بناءً على رقم اللير (y)
                    if (y < 4) {
                        mixerLeft1.gain(y, dynamicGain);      // الليرات 0، 1، 2، 3 للميكسر الأول
                        mixerRight1.gain(y, dynamicGain);
                    } else {
                        mixerLeft2.gain(y - 4, dynamicGain);  // الليرات 4، 5، 6، 7 للميكسر الثاني
                        mixerRight2.gain(y - 4, dynamicGain);
                    }
                    
                    // 🌟 4. عزف السامبل بالمستوى الصوتي الجديد
                    playWav[y].play(sampleBank[sIdx]); 
                }
            } 
        }
    }
    else if (currentMode == LOOPER_MODE) {
        if (rawKnobLengthValue >= 1010 && !loopPlayer.isPlaying() && isPlaying) {
            loopPlayer.play(loopSampleBank[activeLoopIndex]);
        }
    }

    // 🌟 حاسبة الموازير التلقائية (Generative Chord Progression)
    // تفحص الخطوة 0 عند اكتمال مازورة كاملة (دورة السيكوينسر) بشرط أن يكون السيكوينسر في حالة تشغيل
    if (testPlayhead == 0 && isPlaying) {
        barClockCounter++; // زيادة عداد الموازير
        
        if (barClockCounter >= barTargetCount) { // إذا وصلنا إلى 8 موازير
            barClockCounter = 0; // تصفير العداد للبدء من جديد
            
            // اختيار كورد جديد عشوائياً بشرط عدم تكرار نفس الكورد الحالي متتالياً
            int nextChord = currentChordIndex;
            while (nextChord == currentChordIndex) {
                nextChord = random(0, NUM_CHORDS);
            }
            currentChordIndex = nextChord;
            
            updateOLED(); // تحديث شاشة الـ OLED فوراً حياً لعرض اسم الأكورد الجديد
        }
    }

    testPlayhead = (testPlayhead + 1) % looperMaxSteps;
}
/*
void runPhysicsEngine(float sA, float sB) {
    unsigned long now = millis();
    for (int i = 0; i < POLY_SIZE; i++) {
        if (polyA[i].active && (now >= polyA[i].offTime)) { midiSend(0x80, polyA[i].note, 0); polyA[i].active = false; }
        if (polyB[i].active && (now >= polyB[i].offTime)) { midiSend(0x81, polyB[i].note, 0); polyB[i].active = false; }
    }
    if (ballAActive) {
        float nextAX = ballAX + (velAX * sA); float nextAY = ballAY + (velAY * sA); bool hitA = false;
        if (nextAX <= 16.0) { velAX = abs(velAX); ballAX = 16.1; hitA = true; }
        else if (nextAX >= 31.0) { velAX = -abs(velAX); ballAX = 30.9; hitA = true; }
        if (nextAY <= 0.0) { velAY = abs(velAY); ballAY = 0.1; hitA = true; }
        else if (nextAY >= 15.0) { velAY = -abs(velAY); ballAY = 14.9; hitA = true; }
        if (!hitA) {
            int gridX = (int)nextAX; int gridY = (int)nextAY;
            if (gridX >= 16 && gridX < 32 && gridY >= 0 && gridY < 16 && wallMap[gridX][gridY]) {
                hitA = true;
                if (wallMap[gridX][(int)ballAY]) { velAX = -velAX; }
                else if (wallMap[(int)ballAX][gridY]) { velAY = -velAY; }
                else { velAX = -velAX; velAY = -velAY; }
            }
        }
        if (!hitA) { ballAX = nextAX; ballAY = nextAY; }
        if (hitA) {
            byte noteA = rootNotePhysics + chords[currentChordIndex][random(0, CHORD_SIZE)] + (octaveA * 12);
            for (int i = 0; i < POLY_SIZE; i++) if (!polyA[i].active) { polyA[i].note = noteA; polyA[i].offTime = now + globalNoteLengthMs; polyA[i].active = true; midiSend(0x90, noteA, random(65, 100)); break; }
        }
    }
    if (ballBActive) {
        float nextBX = ballBX + (velBX * sB); float nextBY = ballBY + (velBY * sB); bool hitB = false;
        if (nextBX <= 0.0) { velBX = abs(velBX); ballBX = 0.1; hitB = true; }
        else if (nextBX >= 15.0) { velBX = -abs(velBX); ballBX = 14.9; hitB = true; }
        if (nextBY <= 0.0) { velBY = abs(velBY); ballBY = 0.1; hitB = true; }
        else if (nextBY >= 15.0) { velBY = -abs(velBY); ballBY = 14.9; hitB = true; }
        if (!hitB) {
            int gridX = (int)nextBX; int gridY = (int)nextBY;
            if (gridX >= 0 && gridX < 16 && gridY >= 0 && gridY < 16 && wallMap[gridX][gridY]) {
                hitB = true;
                if (wallMap[gridX][(int)ballBX]) { velBX = -velBX; }
                else if (wallMap[(int)ballBX][gridY]) { velBY = -velBY; }
                else { velBX = -velBX; velBY = -velBY; }
            }
        }
        if (!hitB) { ballBX = nextBX; ballBY = nextBY; }
        if (hitB) {
            byte noteB = rootNotePhysics + chords[currentChordIndex][random(0, CHORD_SIZE)] + (octaveB * 12);
            for (int i = 0; i < POLY_SIZE; i++) if (!polyB[i].active) { polyB[i].note = noteB; polyB[i].offTime = now + globalNoteLengthMs; polyB[i].active = true; midiSend(0x91, noteB, random(65, 100)); break; }
        }
    }
}
*/
void runPhysicsEngine(float sA, float sB) {
    unsigned long now = millis();
    
    // 🌟 صمام أمان: إذا كانت المقاومات التماثلية تعطي صفراً، نضع سرعة دنيا تلقائية لكي لا تختفي الكور
    if (sA <= 0.01) sA = 0.3;
    if (sB <= 0.01) sB = 0.3;

    // 🌟 منطق التغيير الزمني (كل 15 ثانية)
    static unsigned long lastChordChange = 0;
    static int currentRandomChord = 0;
    
    if (isChordRandom) {
        if (now - lastChordChange >= 15000) { 
            currentRandomChord = random(0, NUM_CHORDS);
            lastChordChange = now;
        }
    }

    // 🌟 تصحيح منطق استعادة الكرات الفولاذي (Ball Safety Check)
    // نتحقق من الحدود الحقيقية للمصفوفة لمنع الحلقات اللانهائية التي تخفي الكرات
    if (ballAActive && (ballAX < 15.9 || ballAX > 31.1 || ballAY < -0.1 || ballAY > 15.1)) {
        ballAX = 23.0; ballAY = 7.0; velAX = 0.6; velAY = 0.4;
    }
    if (ballBActive && (ballBX < -0.1 || ballBX > 15.1 || ballBY < -0.1 || ballBY > 15.1)) {
        ballBX = 7.0; ballBY = 7.0; velBX = 0.5; velBY = 0.3;
    }

    // 🌟 تصحيح الاختيار العشوائي للأكورد (بدلاً من اللف العشوائي السريع في كل ميكروثانية)
    int activeChord = isChordRandom ? currentRandomChord : currentChordIndex;

    // معالجة إيقاف النوتات (Note Off)
    for (int i = 0; i < POLY_SIZE; i++) {
        if (polyA[i].active && (now >= polyA[i].offTime)) { midiSend(0x80, polyA[i].note, 0); polyA[i].active = false; }
        if (polyB[i].active && (now >= polyB[i].offTime)) { midiSend(0x81, polyB[i].note, 0); polyB[i].active = false; }
    }

    // --- معالجة الكرة A ---
    if (ballAActive) {
        float nextAX = ballAX + (velAX * sA); float nextAY = ballAY + (velAY * sA); bool hitA = false;
        if (nextAX <= 16.0) { velAX = abs(velAX); ballAX = 16.1; hitA = true; }
        else if (nextAX >= 31.0) { velAX = -abs(velAX); ballAX = 30.9; hitA = true; }
        if (nextAY <= 0.0) { velAY = abs(velAY); ballAY = 0.1; hitA = true; }
        else if (nextAY >= 15.0) { velAY = -abs(velAY); ballAY = 14.9; hitA = true; }
        
        if (!hitA) {
            int gridX = (int)nextAX; int gridY = (int)nextAY;
            if (gridX >= 16 && gridX < 32 && gridY >= 0 && gridY < 16 && wallMap[gridX][gridY]) {
                hitA = true;
                if (wallMap[gridX][(int)ballAY]) { velAX = -velAX; }
                else if (wallMap[(int)ballAX][gridY]) { velAY = -velAY; }
                else { velAX = -velAX; velAY = -velAY; }
            }
        }
        if (!hitA) { ballAX = nextAX; ballAY = nextAY; }
        if (hitA) {
            byte noteA = rootNotePhysics + chords[activeChord][random(0, CHORD_SIZE)] + (octaveA * 12);
            for (int i = 0; i < POLY_SIZE; i++) if (!polyA[i].active) { 
                polyA[i].note = noteA; polyA[i].offTime = now + globalNoteLengthMs; 
                polyA[i].active = true; midiSend(0x90, noteA, random(65, 100)); break; 
            }
        }
    }

    // --- معالجة الكرة B ---
    if (ballBActive) {
        float nextBX = ballBX + (velBX * sB); float nextBY = ballBY + (velBY * sB); bool hitB = false;
        if (nextBX <= 0.0) { velBX = abs(velBX); ballBX = 0.1; hitB = true; }
        else if (nextBX >= 15.0) { velBX = -abs(velBX); ballBX = 14.9; hitB = true; }
        if (nextBY <= 0.0) { velBY = abs(velBY); ballBY = 0.1; hitB = true; }
        else if (nextBY >= 15.0) { velBY = -abs(velBY); ballBY = 14.9; hitB = true; }
        
        if (!hitB) {
            int gridX = (int)nextBX; int gridY = (int)nextBY;
            if (gridX >= 0 && gridX < 16 && gridY >= 0 && gridY < 16 && wallMap[gridX][gridY]) {
                hitB = true;
                if (wallMap[gridX][(int)ballBY]) { velBX = -velBX; } // 🌟 تصحيح: الفحص على ballBY للارتداد الرأسي
                else if (wallMap[(int)ballBX][gridY]) { velBY = -velBY; }
                else { velBX = -velBX; velBY = -velBY; }
            }
        }
        if (!hitB) { ballBX = nextBX; ballBY = nextBY; }
        if (hitB) {
            byte noteB = rootNotePhysics + chords[activeChord][random(0, CHORD_SIZE)] + (octaveB * 12);
            for (int i = 0; i < POLY_SIZE; i++) if (!polyB[i].active) { 
                polyB[i].note = noteB; polyB[i].offTime = now + globalNoteLengthMs; 
                polyB[i].active = true; midiSend(0x91, noteB, random(65, 100)); break; 
            }
        }
    }
}


void handleKnobs(unsigned long currentMillis) {
    if (currentMillis - lastAnalogReadTime > 40) {
        lastAnalogReadTime = currentMillis;
        static float smoothedL = analogRead(pinLoopLengthKnob); static float smoothedB = analogRead(pinBPMKnob); static float smoothedV = analogRead(pinVelocityKnob);
        const float filterFactor = 0.15; 
        smoothedL = (smoothedL * (1.0 - filterFactor)) + (analogRead(pinLoopLengthKnob) * filterFactor); smoothedB = (smoothedB * (1.0 - filterFactor)) + (analogRead(pinBPMKnob) * filterFactor); smoothedV = (smoothedV * (1.0 - filterFactor)) + (analogRead(pinVelocityKnob) * filterFactor);
        int intL = (int)smoothedL; rawKnobLengthValue = intL; int intB = (int)smoothedB; int intV = (int)smoothedV;

        if (currentMode == LOOPER_MODE) {
            if (abs(intL - lastLengthKnobValue) > knobThreshold) { 
                lastLengthKnobValue = intL; 
                
                // 🌟 جلب طول الملف الصوتي الحالي حياً من كرت الـ SD
                uint32_t currentLoopMax = loopPlayer.lengthMillis();
                
                // صمام أمان: إذا كان الملف لم يبدأ بعد، نضع 4000 كقيمة مبدئية لمنع تجميد الحسبة
                if (currentLoopMax == 0) {
                    currentLoopMax = 4000; 
                }
                
                // ربط أقصى اليمين بنهاية مقطع الصوت الفعلي ديناميكياً
                looperWindowMs = map(intL, 0, 1010, 60, currentLoopMax); 
            }
            
            // 🌟 كود الـ Cutoff الخاص بك كما هو تماماً دون أي تغيير
            if (abs(intB - lastBPMKnobValue) > knobThreshold) { lastBPMKnobValue = intB; loopCutoffHz = map(intB, 0, 1023, 60, 11500); lowPassFilter.frequency(loopCutoffHz); }
        }
        else if (currentMode == PHYSICS_MODE) {
            if (abs(intL - lastLengthKnobValue) > knobThreshold) lastLengthKnobValue = intL;
            if (abs(intB - lastBPMKnobValue) > knobThreshold) lastBPMKnobValue = intB;
            if (abs(intV - lastVelocityKnobValue) > knobThreshold) { lastVelocityKnobValue = intV; globalNoteLengthMs = map(intV, 0, 1023, 40, 1200); }
        } else {
            if (abs(intL - lastLengthKnobValue) > knobThreshold) { lastLengthKnobValue = intL; totalSteps = map(intL, 0, 1023, 1, 32); looperMaxSteps = totalSteps; }
            if (abs(intB - lastBPMKnobValue) > knobThreshold) { lastBPMKnobValue = intB; stepInterval = (currentMode == SAMPLER_MODE) ? map(intB, 0, 1023, 350, 45) : map(intB, 0, 1023, 450, 35); }
            if (abs(intV - lastVelocityKnobValue) > knobThreshold) { lastVelocityKnobValue = intV; velocityGrid[cursorY][cursorX] = map(intV, 0, 1023, 0, 7); }
        }
    }
}

void handleInterfaceButtons(unsigned long currentMillis) {
    if (currentMillis - lastPress < 150) return;
    if (currentMode == MAIN_MENU) {
        if (digitalRead(btnUp) == LOW || digitalRead(btnLeft) == LOW) { menuSelection = (menuSelection == 0) ? 4 : menuSelection - 1; lastPress = currentMillis; updateOLED(); }
        if (digitalRead(btnDown) == LOW || digitalRead(btnRight) == LOW) { menuSelection = (menuSelection + 1) % 5; lastPress = currentMillis; updateOLED(); }
        if (digitalRead(btnCenter) == LOW) { 
            if (menuSelection == 0) currentMode = MELODIC_MODE; else if (menuSelection == 1) currentMode = RHYTHM_MODE; else if (menuSelection == 2) currentMode = PHYSICS_MODE; else if (menuSelection == 3) currentMode = SAMPLER_MODE;
            else currentMode = LOOPER_MODE;
            cursorX = 0; cursorY = 0; lastPress = currentMillis; updateOLED(); 
        }
        return;
    }
    if (currentMode == PHYSICS_MODE) {
        if (digitalRead(btnCenter) == LOW) { currentSide = (currentSide + 1) % 8; cursorX = walls[currentSide].x; cursorY = walls[currentSide].y; lastPress = currentMillis; updateOLED(); return; }
        int minX = (currentSide < 4) ? 16 : 0; int maxX = (currentSide < 4) ? 31 : 15;
        if (currentSide == 0 || currentSide == 1 || currentSide == 4 || currentSide == 5) {
            if (digitalRead(btnLeft) == LOW && cursorX > minX) { cursorX--; lastPress = currentMillis; }
            if (digitalRead(btnRight) == LOW && cursorX < maxX) { cursorX++; lastPress = currentMillis; }
            if (digitalRead(btnUp) == LOW && walls[currentSide].length < 16) { walls[currentSide].length++; lastPress = currentMillis; } 
            if (digitalRead(btnDown) == LOW && walls[currentSide].length > 0) { walls[currentSide].length--; lastPress = currentMillis; }
        } else {
            if (digitalRead(btnUp) == LOW && cursorY > 0) { cursorY--; lastPress = currentMillis; } 
            if (digitalRead(btnDown) == LOW && cursorY < 15) { cursorY++; lastPress = currentMillis; }
            if (digitalRead(btnLeft) == LOW && walls[currentSide].length > 0) { walls[currentSide].length--; lastPress = currentMillis; }
            if (digitalRead(btnRight) == LOW && walls[currentSide].length < 16) { walls[currentSide].length++; lastPress = currentMillis; }
        }
        walls[currentSide].x = cursorX; walls[currentSide].y = cursorY; updateWallMap();
    } 
    else if (currentMode == LOOPER_MODE) {
        if (digitalRead(btnLeft) == LOW && activeLoopIndex > 0) { activeLoopIndex--; lastPress = currentMillis; if(isPlaying) { loopPlayer.play(loopSampleBank[activeLoopIndex]); looperTimer = millis(); } updateOLED(); }
        if (digitalRead(btnRight) == LOW && activeLoopIndex < 3) { activeLoopIndex++; lastPress = currentMillis; if(isPlaying) { loopPlayer.play(loopSampleBank[activeLoopIndex]); looperTimer = millis(); } updateOLED(); }
    }
    else {
        bool moved = false;
        if (digitalRead(btnLeft) == LOW && cursorX > 0) { cursorX--; moved = true; } 
        if (digitalRead(btnRight) == LOW && cursorX < 31) { cursorX++; moved = true; } 
        if (digitalRead(btnUp) == LOW && cursorY > 0) { cursorY--; moved = true; } 
        if (digitalRead(btnDown) == LOW && cursorY < 7) { cursorY++; moved = true; }
        
        if (digitalRead(btnCenter) == LOW) {
            if (currentMode == MELODIC_MODE) {
                // 🌟 دورة الأوكتافات الهارمونية التناظرية: الأساسي C3 [1] -> الجواب C4 [2] -> القرار C2 [0] -> إطفاء [Off]
                if (!gridRB[cursorY][cursorX]) {
                    for(int y=0; y<8; y++) { gridRB[y][cursorX] = false; } // مونو صارم لطبقة الميلوديك
                    gridRB[cursorY][cursorX] = true;
                    octaveGrid[cursorY][cursorX] = 1;  
                    velocityGrid[cursorY][cursorX] = 5;
                } 
                else {
                    if (octaveGrid[cursorY][cursorX] == 1)      octaveGrid[cursorY][cursorX] = 2; 
                    else if (octaveGrid[cursorY][cursorX] == 2) octaveGrid[cursorY][cursorX] = 0; 
                    else {
                        gridRB[cursorY][cursorX] = false;
                        octaveGrid[cursorY][cursorX] = 0;
                        velocityGrid[cursorY][cursorX] = 0;
                    }
                }
            } 
            else if (currentMode == RHYTHM_MODE) {
                // 🌟 إصلاح الطبول الصارم والمنفصل تماماً عن الإنكودر:
                // التفعيل والإلغاء مقيد بالسطر/اللير المحدد بواسطة أزرار الاتجاهات (cursorY)
                gridRB[cursorY][cursorX] = !gridRB[cursorY][cursorX];
                if (gridRB[cursorY][cursorX]) {
                    velocityGrid[cursorY][cursorX] = 5; // الفلوسيتي الافتراضي للضربة الإيقاعية
                } else {
                    velocityGrid[cursorY][cursorX] = 0;
                }
            }
            else { 
                // وضع السامبلر وبقية الأقسام المعتمدة نقراً عادياً Toggle
                gridRB[cursorY][cursorX] = !gridRB[cursorY][cursorX]; 
                if(gridRB[cursorY][cursorX]) velocityGrid[cursorY][cursorX] = 5; 
            }
            moved = true;
        }
        if (moved) { lastPress = currentMillis; updateOLED(); }
    }
    if (digitalRead(pinPresetUp) == LOW) { currentPreset = (currentPreset + 1) % 100; if(currentPreset==0) currentPreset=1; updateMatrixDisplay(currentPreset); lastPress = currentMillis; updateOLED(); }
    if (digitalRead(pinPresetDown) == LOW) { currentPreset = (currentPreset == 1) ? 99 : currentPreset - 1; updateMatrixDisplay(currentPreset); lastPress = currentMillis; updateOLED(); }
    if (digitalRead(pinSave) == LOW) { savePresetToSD(currentPreset); lastPress = currentMillis; }
    if (digitalRead(pinLoad) == LOW) { allNotesOff(); loadPresetFromSD(currentPreset); lastPress = currentMillis; updateOLED(); }
}

void handleEncoder() {
    int currClk = digitalRead(clkPin);
    if (currClk != lastClk) {
        if (currClk == LOW) { 
            int dir = (digitalRead(dtPin) != LOW) ? 1 : -1;
            if (currentMode == MAIN_MENU) menuSelection = (dir == 1) ? (menuSelection + 1) % 5 : (menuSelection == 0 ? 4 : menuSelection - 1);
            else if (currentMode == MELODIC_MODE) {
                if (melodicMenuIndex == 0) { currentInstrument = constrain(currentInstrument + dir, 0, 127); midiSend(0xC0 | melodicChannel, currentInstrument, 0); }
                else if (melodicMenuIndex == 1) currentScale = constrain(currentScale + dir, 0, 4);
                else if (melodicMenuIndex == 2) rootNoteMelodic = constrain(rootNoteMelodic + dir, 24, 84);
                else if (melodicMenuIndex == 3) noteLengthPercent = constrain(noteLengthPercent + (dir * 5), 10, 100);
            } 
            else if (currentMode == RHYTHM_MODE) layerNotes[activeRhythmLayer] = constrain(layerNotes[activeRhythmLayer] + dir, 0, 127);
            else if (currentMode == SAMPLER_MODE) { 
                
                samplerNotes[oledActiveLine] = constrain(samplerNotes[oledActiveLine] + dir, 0, 20);
            }
            else if (currentMode == LOOPER_MODE) { activeLoopIndex = constrain(activeLoopIndex + dir, 0, 3); if (isPlaying) { loopPlayer.play(loopSampleBank[activeLoopIndex]); looperTimer = millis(); } }
           else if (currentMode == PHYSICS_MODE) {
        if (editModePhysics == 0) ballAActive = !ballAActive;
           else if (editModePhysics == 1) ballBActive = !ballBActive;
          else if (editModePhysics == 2) { 
          pluckedIndexA = constrain(pluckedIndexA + dir, 0, NUM_PLUCKED_INST - 1); 
          sendInstrumentChange(0, pluckedInstruments[pluckedIndexA]); 
          }
          else if (editModePhysics == 3) { 
            pluckedIndexB = constrain(pluckedIndexB + dir, 0, NUM_PLUCKED_INST - 1); 
           sendInstrumentChange(1, pluckedInstruments[pluckedIndexB]); 
            }
            else if (editModePhysics == 4) {
           // إذا كان الراندوم مطفأ، الإنكودر يغير الكوردات
          // إذا كان الراندوم مفعلاً، الإنكودر لا يفعل شيئاً (يمنع التغيير أثناء العشوائية)
           if (!isChordRandom) {
            currentChordIndex = (currentChordIndex + dir + NUM_CHORDS) % NUM_CHORDS;
            }
            }
            else if (editModePhysics == 5) {isChordRandom = !isChordRandom;}
           else if (editModePhysics == 6) rootNotePhysics = constrain(rootNotePhysics + dir, 36, 84);
          else if (editModePhysics == 7) octaveA = constrain(octaveA + dir, -2, 2);
          else if (editModePhysics == 8) octaveB = constrain(octaveB + dir, -2, 2);
          
}
            updateOLED();
        }
        lastClk = currClk;
    }
    int swVal = digitalRead(swPin);
    if (swVal == LOW && lastSwState == HIGH) swPressedTime = millis();
    if (swVal == HIGH && lastSwState == LOW) {
        unsigned long holdDuration = millis() - swPressedTime;
        if (holdDuration > 600) { currentMode = MAIN_MENU; allNotesOff(); isPlaying = false; cursorX = 0; cursorY = 0; if (loopPlayer.isPlaying()) loopPlayer.stop(); for(int y=0; y<8; y++) if(playWav[y].isPlaying()) playWav[y].stop(); }
        else if (holdDuration > 40) {
            if (currentMode == MAIN_MENU) {
                if (menuSelection == 0) currentMode = MELODIC_MODE; else if (menuSelection == 1) currentMode = RHYTHM_MODE; else if (menuSelection == 2) currentMode = PHYSICS_MODE; else if (menuSelection == 3) currentMode = SAMPLER_MODE;
                else currentMode = LOOPER_MODE; cursorX = 0; cursorY = 0;
            }
            else if (currentMode == MELODIC_MODE) melodicMenuIndex = (melodicMenuIndex + 1) % 5;
            else if (currentMode == RHYTHM_MODE) activeRhythmLayer = (activeRhythmLayer + 1) % 8;
            else if (currentMode == SAMPLER_MODE) oledActiveLine = (oledActiveLine + 1) % 8;
            else if (currentMode == PHYSICS_MODE) editModePhysics = (editModePhysics + 1) % 8;
        }
        updateOLED(); lastPress = millis();
    }
    lastSwState = swVal;
}

void updateOLED() {
    oled.clearDisplay(); oled.setTextSize(1); oled.setTextColor(WHITE); oled.setCursor(0, 0);
    if (currentMode == MAIN_MENU) {
        oled.println("    === MUSIC BOX ==="); oled.drawFastHLine(0, 10, 128, WHITE); oled.println(""); 
        if (menuSelection < 4) { oled.println(menuSelection == 0 ? "> MELODIC SEQ" : "  MELODIC SEQ"); oled.println(menuSelection == 1 ? "> RHYTHM BOX" : "  RHYTHM BOX"); oled.println(menuSelection == 2 ? "> BOUNCE PHYSICS" : "  BOUNCE PHYSICS"); oled.println(menuSelection == 3 ? "> TEENSY SAMPLER" : "  TEENSY SAMPLER"); }
        else { oled.println("  BOUNCE PHYSICS"); oled.println("  TEENSY SAMPLER"); oled.println(menuSelection == 4 ? "> MIDI SYNC LOOPER" : "  MIDI SYNC LOOPER"); }
        oled.setCursor(0, 56); oled.print("MIDI SYNC : "); oled.print(externalSync ? "EXT" : "INT");
    }
    else if (currentMode == PHYSICS_MODE) {
        oled.print("BOUNCE | Side:"); oled.println(currentSide); oled.drawFastHLine(0, 9, 128, WHITE);
        
        oled.setCursor(2, 12);  oled.print(editModePhysics == 0 ? ">BalA:" : " BalA:"); oled.print(ballAActive ? "ON" : "OFF");
        oled.setCursor(68, 12); oled.print(editModePhysics == 1 ? ">BalB:" : " BalB:"); oled.print(ballBActive ? "ON" : "OFF");
        
        oled.setCursor(2, 23);  oled.print(editModePhysics == 2 ? ">InsA:" : " InsA:"); oled.print(pluckedNames[pluckedIndexA]);
        oled.setCursor(68, 23); oled.print(editModePhysics == 3 ? ">InsB:" : " InsB:"); oled.print(pluckedNames[pluckedIndexB]);
        
        oled.setCursor(2, 34);  oled.print(editModePhysics == 4 ? ">Chrd:" : " Chrd:"); oled.print(chordNames[currentChordIndex]);
        
        // 🌟 هنا التعديل: الخيار رقم 5 الجديد للراندوم
        oled.setCursor(68, 34); oled.print(editModePhysics == 5 ? ">RND :" : " RND :"); oled.print(isChordRandom ? "ON" : "OFF");
        
        oled.setCursor(2, 45);  oled.print(editModePhysics == 6 ? ">Root:" : " Root:"); oled.print(rootNotePhysics);
        
        oled.setCursor(2, 56);  oled.print(editModePhysics == 7 ? ">OctA:" : " OctA:"); oled.print(octaveA >= 0 ? "+" : ""); oled.print(octaveA);
        oled.setCursor(68, 56); oled.print(editModePhysics == 8 ? ">OctB:" : " OctB:"); oled.print(octaveB >= 0 ? "+" : ""); oled.print(octaveB);
    }
    else if (currentMode == LOOPER_MODE) {
        oled.print("AUDIO LOOPER | P:"); oled.println(currentPreset); oled.drawFastHLine(0, 10, 128, WHITE); oled.println(""); 
        oled.print(" ACTIVE LOOP: "); oled.println(loopSampleBank[activeLoopIndex]); oled.drawFastHLine(0, 32, 128, WHITE); oled.println("");
        oled.print(" LPF CUTOFF : "); oled.print(loopCutoffHz); oled.println(" Hz");
        oled.print(" LOOP TIME  : "); if (rawKnobLengthValue >= 1010) oled.println("FREE"); else { oled.print(looperWindowMs); oled.println(" ms"); }
    }
    else if (currentMode == MELODIC_MODE) {
        oled.print("MELODIC | P:"); oled.println(currentPreset); oled.drawFastHLine(0, 10, 128, WHITE);
        const char* mLabels[] = {"INST", "SCALE", "ROOT", "LEN", "MODE"};
        for(int i=0; i<5; i++) { oled.setCursor(0, 15+(i*10)); oled.print(melodicMenuIndex == i ? "> " : "  "); oled.print(mLabels[i]); oled.print(": "); if(i==0) oled.print(currentInstrument); else if(i==1) oled.print(scaleNames[currentScale]); else if(i==2) oled.print(rootNoteMelodic); else if(i==3) oled.print(noteLengthPercent); else oled.print(modeNames[playModeMelodic]); }
    }
    else if (currentMode == RHYTHM_MODE) {
        oled.print("RHYTHM | P:"); oled.println(currentPreset); oled.drawFastHLine(0, 10, 128, WHITE);
        for(int i=0; i<8; i++) { oled.setCursor((i<4?0:64), 15+((i%4)*12)); oled.print(activeRhythmLayer == i ? ">L" : " L"); oled.print(i+1); oled.print(":"); oled.print(layerNotes[i]); }
    }
    else if (currentMode == SAMPLER_MODE) {
        oled.print("SAMPLERBOX | P:"); oled.println(currentPreset); oled.drawFastHLine(0, 10, 128, WHITE); 
        // 🌟 التعديل هنا: غيرنا layerNotes[i] إلى samplerNotes[i] ليقرأ الأرقام المستقلة للسامبلر
        for (int i = 0; i < 4; i++) { oled.setCursor(5, i * 12 + 15); oled.print(oledActiveLine == i ? ">SL" : " SL"); oled.print(i + 1); oled.print(":"); oled.print(samplerNotes[i]); }
        for (int i = 4; i < 8; i++) { oled.setCursor(68, (i - 4) * 12 + 15); oled.print(oledActiveLine == i ? ">SL" : " SL"); oled.print(i + 1); oled.print(":"); oled.print(samplerNotes[i]); }
    }
    oled.display();
}

void renderLedsOutput() {
    for(int i=0; i<NUM_LEDS; i++) { ledsM1[i] = CRGB::Black; ledsM2[i] = CRGB::Black; }
    uint8_t globalHue = (millis() / 35) % 256; bool cursorBlinkState = (millis() / 250) % 2 == 0;
    
    if (currentMode == MAIN_MENU) {
        runPhysicsEngine(0.3, 0.3); CRGB lowBlue = CRGB(0, 1, 4); CRGB lowRed = CRGB(4, 0, 0); 
        for(int i = 0; i <= 15; i++) { setUnifiedPixelPhysics(i, 0, lowBlue); setUnifiedPixelPhysics(i, 15, lowBlue); setUnifiedPixelPhysics(0, i, lowBlue); setUnifiedPixelPhysics(15, i, lowBlue); }
        for(int i = 16; i <= 31; i++) { setUnifiedPixelPhysics(i, 0, lowRed); setUnifiedPixelPhysics(i, 15, lowRed); }
        for(int i = 0; i <= 15; i++) { setUnifiedPixelPhysics(16, i, lowRed); setUnifiedPixelPhysics(31, i, lowRed); }
        for(int x = 0; x < 32; x++) for(int y = 0; y < 16; y++) if(wallMap[x][y]) setUnifiedPixelPhysics(x, y, CRGB(15, 0, 5)); 
        if (ballAActive) setUnifiedPixelPhysics((int)ballAX, (int)ballAY, CRGB(0, 20, 30));   
        if (ballBActive) setUnifiedPixelPhysics((int)ballBX, (int)ballBY, CRGB(30, 0, 20));   

        char w1[] = "MUSIC"; int xP1 = 2; for (int i = 0; i < 5; i++) { CRGB letterColor = CHSV(globalHue + (i * 20), 240, 110); drawLetter(w1[i], xP1, 1, letterColor, ledsM2); xP1 += (w1[i] == 'I') ? 4 : 6; }
        char w2[] = "BOX"; int xP2 = 7; for (int i = 0; i < 3; i++) { CRGB letterColor = CHSV(globalHue + 100 + (i * 25), 240, 110); drawLetter(w2[i], xP2, 1, letterColor, ledsM1); xP2 += 6; }
    } 
    else if (currentMode == PHYSICS_MODE) {
        // 🌟 صمام أمان عاجل: إذا هربت الكرات خارج حدود المصفوفة (0-31 للـ X ومن 0-15 للـ Y) أعدها للمنتصف فوراً
        if (ballAX < 0.0 || ballAX > 31.0 || ballAY < 0.0 || ballAY > 15.0) { ballAX = 23.0; ballAY = 7.0; velAX = 0.6; velAY = 0.4; }
        if (ballBX < 0.0 || ballBX > 15.0 || ballBY < 0.0 || ballBY > 15.0) { ballBX = 7.0;  ballBY = 7.0; velBX = 0.5; velBY = 0.3; }

        // كود الرسم الخاص بك كما هو...
        for(int i = 0; i < 16; i++) {
            setUnifiedPixelPhysics(0, i, CRGB(30, 0, 0));  
            setUnifiedPixelPhysics(15, i, CRGB(30, 0, 0)); 
            setUnifiedPixelPhysics(16, i, CRGB(30, 0, 0)); 
            setUnifiedPixelPhysics(31, i, CRGB(30, 0, 0)); 
        }
        for(int i = 0; i < 32; i++) {
            setUnifiedPixelPhysics(i, 0, CRGB(30, 0, 0));  
            setUnifiedPixelPhysics(i, 15, CRGB(30, 0, 0)); 
        }
        for(int x=0; x<32; x++) for(int y=0; y<16; y++) if(wallMap[x][y]) setUnifiedPixelPhysics(x, y, CRGB(140, 0, 50)); 
        if (cursorBlinkState) setUnifiedPixelPhysics(cursorX, cursorY, CRGB::Yellow); 
        if (ballAActive) setUnifiedPixelPhysics((int)ballAX, (int)ballAY, CRGB::Cyan);   
        if (ballBActive) setUnifiedPixelPhysics((int)ballBX, (int)ballBY, CRGB::Magenta); 
    }
   else if (currentMode == LOOPER_MODE) {
        // 🌟 1. قراءة مستوى طاقة الصوت الحية الفعلية (UV/Peak) من الإشارة (تعطي قيمة بين 0.0 و 1.0)
        float rawVolume = 0.0;
        if (loopPeakAnalyzer.available()) {
            rawVolume = loopPeakAnalyzer.read();
        }

        // 🌟 2. تحويل القراءة إلى استجابة لوغاريتمية (بالميكرو ثانية) لمحاكاة حركة مؤشرات الـ dB الاحترافية
        // نقوم بعمل تنعيم (Smoothing) خفيف لحركة الأعمدة لتتحرك بسلاسة عند الهبوط
        static float smoothedVolume = 0.0;
        if (rawVolume > smoothedVolume) {
            smoothedVolume = rawVolume; // صعود سريع مع النبرة الصدمية للصوت (Attack)
        } else {
            smoothedVolume = (smoothedVolume * 0.85) + (rawVolume * 0.15); // هبوط سلس (Release)
        }

        int playheadX = testPlayhead; // خط التزامن الحالي

        // 🌟 3. رسم الطيف المتراقص بناءً على مستوى طاقة الصوت الحقيقية
        for (int x = 0; x < 32; x++) {
            // توليد تفاوت عمودي طفيف (Micro-variations) لكل عمود ليظهر كطيف حقيقي متحرك وليس كتلة واحدة الثبات
            float columnFactor = ((x % 3 == 0) ? 1.0 : (x % 5 == 0) ? 0.8 : (x % 2 == 0) ? 0.6 : 0.9);
            
            // حساب الارتفاع الفعلي للعمود (من 1 إلى 8 خطوات) بناءً على الـ dB الحقيقي والـ Factor
            int finalHeight = (int)(smoothedVolume * 8.0 * columnFactor * 1.4);
            finalHeight = constrain(finalHeight, 1, 8);

            // خط السيكوينسر (Playhead) يضيء بالكامل لبيان التزامن الموسيقي
            if (x == playheadX && isPlaying) finalHeight = 8;

            // توليد ألوان قزحية تتحرك مع الزمن
            CRGB col = CHSV(globalHue + (x * 7), 255, 210);

            // 🌟 4. الرسم في المصفوفة العلوية M2 (يرتفع من الأسفل 7 إلى الأعلى)
            for (int h = 0; h < finalHeight; h++) { 
                ledsM2[getMatrixXY(x, 7 - h)] = col; 
            }

            // 🌟 5. الانعكاس المرآتي الكامل (Flip) في المصفوفة السفلية M1 (ينزل من الأعلى 0 إلى الأسفل)
            for (int h = 0; h < finalHeight; h++) { 
                ledsM1[getMatrixXY(x, h)] = col; 
            }
        }
    }
    else {
        // 1. عرض المصفوفة العلوية لنقاط العزف والخطوات الزمنية
        for (int x = 0; x < 32; x++) {
            for (int y = 0; y < 8; y++) {
                if (gridRB[y][x]) {
                    // 🌟 إذا كنا في قسم الطبول يأخذ النقاط المفعّلة لون الطبل الثابت الخاص بها حياً
                    CRGB col = (currentMode == RHYTHM_MODE) ? layerColors[y] : CHSV(globalHue + (y * 22) + (x * 4), 250, 180);
                    if (x == testPlayhead && isPlaying) { col.r = qadd8(col.r, 120); col.g = qadd8(col.g, 120); col.b = qadd8(col.b, 120); }
                    ledsM2[getMatrixXY(x, y)] = col;
                } else { if (x == testPlayhead && y == 0 && isPlaying) ledsM2[getMatrixXY(x, 0)] = CRGB(0, 45, 20); }
            }
        }
        
        if (cursorBlinkState) ledsM2[getMatrixXY(cursorX, cursorY)] = CRGB(120, 120, 120); 
        for (int x = 0; x < totalSteps; x++) ledsM1[getMatrixXY(x, 0)] = (x % 4 == 0) ? CRGB::Red : CRGB::Blue;
        
        // 2. عرض شاشة الفلوسيتي الملونة والمخصصة سفلية (ledsM1)
        for (int x = 0; x < 32; x++) {
            if (currentMode == RHYTHM_MODE || currentMode == SAMPLER_MODE) {
                // 🌟 وضع الطبول المطور: يعرض فقط شريط الفلوسيتي الخاص باللير الذي تقف عليه حالياً بالملي وثبات تلويني كامل
                if (gridRB[cursorY][x]) {
                    int h = velocityGrid[cursorY][x];
                    CRGB drumColor = layerColors[cursorY];
                    for (int vY = 0; vY < h; vY++) ledsM1[getMatrixXY(x, 7 - vY)] = drumColor;
                }
            } else {
                // الأقسام الموسيقية الأخرى (الميلوديك والسامبلر) تعود لنظامها الأصلي القزحي
                int activeY = -1; for (int y = 0; y < 8; y++) { if (gridRB[y][x]) { activeY = y; break; } }
                if (activeY != -1) {
                    int h = velocityGrid[activeY][x]; 
                    CRGB barColor = CHSV(globalHue + (activeY * 22) + (x * 4), 240, 160);
                    for (int vY = 0; vY < h; vY++) ledsM1[getMatrixXY(x, 7 - vY)] = barColor; 
                }
            }
        }
    }
    FastLED.show();
}



void onClock() { externalSync = true; lastClockMillis = millis(); clockTicks++; if (clockTicks >= 6) { clockTicks = 0; if (isPlaying && currentMode != PHYSICS_MODE) runSequencerEngine(); } }
void onStart() { testPlayhead = 0; clockTicks = 0; dawRunning = true; isPlaying = true; }
void onStop() { dawRunning = false; isPlaying = false; allNotesOff(); if(loopPlayer.isPlaying()) loopPlayer.stop(); for(int y=0; y<8; y++) if(playWav[y].isPlaying()) playWav[y].stop(); }
void onContinue() { dawRunning = true; isPlaying = true; }
// 🌟 إعلان أولي للدوال الموجودة في ملف function.ino
void setupLEDs();
void updateModeLEDs();

void setup() {
    Serial.begin(9600); Serial1.begin(31250); Wire.begin(); Wire.setClock(400000);
    usbMIDI.setHandleClock(onClock); usbMIDI.setHandleStart(onStart); usbMIDI.setHandleStop(onStop); usbMIDI.setHandleContinue(onContinue);
    AudioMemory(60); 
    mixerLeft1.gain(0, 0.5);  mixerLeft1.gain(1, 0.5);  mixerLeft1.gain(2, 0.5);  mixerLeft1.gain(3, 0.5);
    mixerLeft2.gain(0, 0.5);  mixerLeft2.gain(1, 0.5);  mixerLeft2.gain(2, 0.5);  mixerLeft2.gain(3, 0.5);
    mixerRight1.gain(0, 0.5); mixerRight1.gain(1, 0.5); mixerRight1.gain(2, 0.5); mixerRight1.gain(3, 0.5);
    mixerRight2.gain(0, 0.5); mixerRight2.gain(1, 0.5); mixerRight2.gain(2, 0.5); mixerRight2.gain(3, 0.5);
    finalMixLeft.gain(0, 0.8);  finalMixLeft.gain(1, 0.8); finalMixLeft.gain(2, 0.8);
    finalMixRight.gain(0, 0.8); finalMixRight.gain(1, 0.8); finalMixRight.gain(2, 0.8);
    lowPassFilter.frequency(4000); lowPassFilter.resonance(1.2);

    for(int i=0; i<4; i++) colors[i] = CRGB(octaveColors[i]);
    if(oled.begin(SSD1306_SWITCHCAPVCC, 0x3C)) { oled.clearDisplay(); updateOLED(); }

    pinMode(btnPlayStop, INPUT_PULLUP); pinMode(pinPresetUp, INPUT_PULLUP); pinMode(pinPresetDown, INPUT_PULLUP); pinMode(pinSave, INPUT_PULLUP); pinMode(pinLoad, INPUT_PULLUP); pinMode(clkPin, INPUT_PULLUP); pinMode(dtPin, INPUT_PULLUP); pinMode(swPin, INPUT_PULLUP);
    for(int i = 5; i <= 9; i++) pinMode(i, INPUT_PULLUP);

    lastClk = digitalRead(clkPin); FastLED.addLeds<WS2812B, PIN_M1, GRB>(ledsM1, NUM_LEDS); FastLED.addLeds<WS2812B, PIN_M2, GRB>(ledsM2, NUM_LEDS); FastLED.setBrightness(25);
    matrixDisplay.shutdown(0, false); matrixDisplay.setIntensity(0, 5); matrixDisplay.clearDisplay(0);
    if (SD.begin(BUILTIN_SDCARD)) sdCardReady = true;
    for(int y=0; y<8; y++) for(int x=0; x<32; x++) { velocityGrid[y][x] = 5; octaveGrid[y][x] = 1; }
    for(int i=0; i<POLY_SIZE; i++) { polyA[i].active = false; polyB[i].active = false; }
    analogReadAveraging(32);

    sendInstrumentChange(0, pluckedInstruments[pluckedIndexA]); sendInstrumentChange(1, pluckedInstruments[pluckedIndexB]); 
    currentPreset = EEPROM.read(50); if(currentPreset > 99 || currentPreset < 1) currentPreset = 1;
    loadPresetFromSD(currentPreset); updateMatrixDisplay(currentPreset); updateOLED();

    
    setupLEDs();
}

void loop() {
    usbMIDI.read(); unsigned long currentMillis = millis();
    if (externalSync && (currentMillis - lastClockMillis > 500)) { externalSync = false; updateOLED(); }
    if (matrixBlinkActive && (currentMillis - matrixBlinkTimer > 150)) { matrixDisplay.shutdown(0, false); updateMatrixDisplay(currentPreset); matrixBlinkActive = false; }
    handleKnobs(currentMillis); handleEncoder(); handleInterfaceButtons(currentMillis);

    if (currentMode == LOOPER_MODE && isPlaying && loopPlayer.isPlaying()) {
        if (rawKnobLengthValue < 1010) {
            if (currentMillis - looperTimer >= (unsigned long)looperWindowMs) {
                loopPlayer.play(loopSampleBank[activeLoopIndex]); looperTimer = currentMillis; updateOLED(); 
            }
        } else {
            if (!loopPlayer.isPlaying()) { loopPlayer.play(loopSampleBank[activeLoopIndex]); }
        }
    }

    updateModeLEDs();


    int ps = digitalRead(btnPlayStop);
    if (ps == LOW && lastPlayStopState == HIGH && (currentMillis - lastPress > 300)) { 
        isPlaying = !isPlaying; 
        if (!isPlaying) { allNotesOff(); if(loopPlayer.isPlaying()) loopPlayer.stop(); for(int y=0; y<8; y++) if(playWav[y].isPlaying()) playWav[y].stop(); }
        else { if (currentMode == LOOPER_MODE) { loopPlayer.play(loopSampleBank[activeLoopIndex]); looperTimer = currentMillis; } }
        lastPress = currentMillis; 
    }
    lastPlayStopState = ps;

    if (currentMode == PHYSICS_MODE) {
        if (isPlaying) {
            float speedBallA = map(analogRead(pinLoopLengthKnob), 0, 1023, 10, 150) / 100.0;
            float speedBallB = map(analogRead(pinBPMKnob), 0, 1023, 10, 150) / 100.0;
            runPhysicsEngine(speedBallA, speedBallB);
        }
    } else {
        if (!externalSync && isPlaying && (currentMillis - lastStepTime >= stepInterval)) { runSequencerEngine(); lastStepTime = currentMillis; }
        if (currentMode == MELODIC_MODE && isPlaying && noteIsActiveInStep) {
            if (currentMillis - lastStepTime >= (stepInterval * noteLengthPercent / 100)) { midiSend(0x80 | melodicChannel, activeNotePlaying, 0); noteIsActiveInStep = false; }
        }
    }
    renderLedsOutput();
    if (currentMode == PHYSICS_MODE || currentMode == MAIN_MENU) delay(15); 
}

// ==========================================
// 🌟 كود التحكم في الـ 5 LEDs الخاص بـ Teensy 4.1
// ==========================================

//const int modeLEDPins[5] = {34, 35, 36, 37, 38}; 

void setupLEDs() {
    for (int i = 0; i < 5; i++) {
        pinMode(modeLEDPins[i], OUTPUT);
        digitalWrite(modeLEDPins[i], HIGH); // إضاءة فحص عند التشغيل
    }
    delay(400); // نصف ثانية تقريباً لترى الإضاءة كهربائياً
    for (int i = 0; i < 5; i++) {
        digitalWrite(modeLEDPins[i], LOW);  // إطفاء الجميع لبدء العمل الطبيعي
    }
}

void updateModeLEDs() {
    static int lastActiveIndex = -1; 
    
    // 🌟 الإصلاح هنا: إذا كنا في المنيو الرئيسي نتبع حركة المؤشر (menuSelection)
    // أما إذا دخلنا أي قسم، نثبت الإضاءة على قيمة الـ currentMode الحالي
    int activeIndex = currentMode;
    if (currentMode == MAIN_MENU) {
        activeIndex = menuSelection;
    } else {
        // الخيارات في الـ currentMode غالباً تبدأ من 1 (أو تختلف عن ترتيب المنيو)
        // تأكد من ربط قيم الـ currentMode بالأرقام من 0 إلى 4:
        if (currentMode == MELODIC_MODE) activeIndex = 0;
        else if (currentMode == RHYTHM_MODE) activeIndex = 1; // أو RYTHM_MODE بحسب اسم المتغير عندك
        else if (currentMode == PHYSICS_MODE) activeIndex = 2;
        else if (currentMode == SAMPLER_MODE) activeIndex = 3;
        else if (currentMode == LOOPER_MODE) activeIndex = 4;
    }
    
    // تحديث الدبابيس فقط عند حدوث تغيير حقيقي في القسم المختار
    if (activeIndex != lastActiveIndex && activeIndex >= 0 && activeIndex < 5) {
        for (int i = 0; i < 5; i++) {
            if (i == activeIndex) {
                digitalWrite(modeLEDPins[i], HIGH); // تثبيت الإضاءة على القسم الفعلي المستقر
            } else {
                digitalWrite(modeLEDPins[i], LOW);  
            }
        }
        lastActiveIndex = activeIndex; 
    }
}