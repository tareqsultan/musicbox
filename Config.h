#ifndef CONFIG_H
#define CONFIG_H

// أبعاد مصفوفات الليدات الثابتة بالهاردوير
#define MW 32
#define MH 8
#define NUM_LEDS (MW * MH)

// أرجل التحكم في مصفوفات الليدات
#define PIN_M1 2
#define PIN_M2 3

// أبعاد شاشة الـ OLED
#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64

// توزيع الأرجل القديم القياسي والمستقر للأزرار الخمسة
const int btnUp = 5;
const int btnDown = 6;
const int btnLeft = 7;
const int btnRight = 8;
const int btnCenter = 9; 

// أرجل المقاومتين التماثليتين المشتركتين
const int pinLoopLengthKnob = A0; 
const int pinBPMKnob = A1;        
const int pinVelocityKnob = A2;   

// زر التشغيل والإيقاف القياسي
const int btnPlayStop = 20;


// مصفوفة مستقلة لـ Samplerbox مخزن فيها افتراضياً الصوت رقم 0 لكل سطر من الأسطر الثمانية
int samplerNotes[8] = {0, 0, 0, 0, 0, 0, 0, 0};

const int modeLEDPins[5] = {34, 35, 36, 37, 38};


// أرجل الإنكودر الدوار القياسية
const int clkPin = 23;
const int dtPin = 22;
const int swPin = 21; 

// أرجل التحكم في شاشة البريست والحفظ (LedControl & Buttons)
const int pinPresetUp = 27;   
const int pinPresetDown = 28;
const int pinSave = 32;       
const int pinLoad = 33;       

// عتبة قراءة ضوضاء المقاومات
const int knobThreshold = 2; 

// ألوان الأوكتافات الثلاثة الكاملة للنقطة العليا المضيئة
const uint32_t octaveColors[4] = {
  0x000000, // Black
  0x005F00, // C2: أخضر
  0x5F2D00, // C3: برتقالي
  0x5F003C  // C4: بنفسجي
};

// فواصل السلالم الموسيقية للسينث
const int scaleIntervals[5][8] = {
  {0, 2, 4, 5, 7, 9, 11, 12}, // Major
  {0, 2, 3, 5, 7, 8, 10, 12}, // Minor
  {0, 2, 3, 5, 7, 9, 10, 12}, // Dorian
  {0, 2, 4, 5, 7, 9, 10, 12}, // Myxolydian
  {0, 3, 5, 6, 7, 10, 12, 15} // Blues
};

// مصفوفة الأرقام لشاشة السيفسج (LedControl)
const byte matrixDigits[10][5] = {
  {B111, B101, B101, B101, B111}, {B010, B110, B010, B010, B111}, 
  {B111, B001, B111, B100, B111}, {B111, B001, B111, B001, B111}, 
  {B101, B101, B111, B001, B001}, {B111, B100, B111, B001, B111}, 
  {B111, B100, B111, B101, B111}, {B111, B001, B010, B010, B010}, 
  {B111, B101, B111, B101, B111}, {B111, B101, B111, B001, B111}  
};

// ================================================================
// --- إعدادات النظام الصوتي العتادي الموازي لـ Teensy (9 قنوات) ---
// ================================================================
#include <Audio.h>
#include <SPI.h>
#include <SerialFlash.h>

#define NUM_SAMPLE_LAYERS 8   
extern AudioPlaySdWav          playWav[NUM_SAMPLE_LAYERS]; 
extern AudioPlaySdRaw          loopPlayer;       
extern AudioFilterStateVariable lowPassFilter;    
extern AudioMixer4              mixerLeft1;  
extern AudioMixer4              mixerLeft2;  
extern AudioMixer4              mixerRight1; 
extern AudioMixer4              mixerRight2; 
extern AudioMixer4              finalMixLeft;
extern AudioMixer4              finalMixRight;
extern AudioOutputMQS          mqsAudioOut; 

extern AudioPlaySdResmp          loopPlayer; // 🌟 النوع الجديد المتغير السرعة

// 🌟 التعديل: تحويل كابلات التوصيل إلى extern لمنع تكرارها في الذاكرة
extern AudioConnection          patchL0, patchL1, patchL2, patchL3, patchL4, patchL5, patchL6, patchL7;
extern AudioConnection          patchR0, patchR1, patchR2, patchR3, patchR4, patchR5, patchR6, patchR7;
extern AudioConnection          mergeL1, mergeL2, mergeR1, mergeR2;
extern AudioConnection          patchLoopToFilter, patchFilterToFinalL, patchFilterToFinalR;
extern AudioConnection          patchOutL, patchOutR;

// الألوان الثابتة (كما هي)
const CRGB layerColors[8] = {
    CRGB(0, 120, 0), CRGB(60, 0, 120), CRGB(120, 0, 60), CRGB(0, 90, 120),
    CRGB(0, 0, 120), CRGB(100, 100, 0), CRGB(120, 50, 0), CRGB(80, 80, 80)
};

#endif