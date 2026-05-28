/*
void savePresetToSD(int presetNum) {
    if (!sdCardReady) return;
    char fileName[13]; 
    
    // --- 1. معالجة وحفظ ملف الـ Preset الأساسي ---
    sprintf(fileName, "P%02d.BIN", presetNum);
    if (SD.exists(fileName)) { SD.remove(fileName); } // تصفير ومسح الملف القديم تماماً لضمان عدم تداخل البيانات
    
    File file = SD.open(fileName, FILE_WRITE);
    if (file) { 
        file.write((uint8_t*)gridRB, sizeof(gridRB)); 
        file.write((uint8_t*)velocityGrid, sizeof(velocityGrid)); 
        file.write((uint8_t*)octaveGrid, sizeof(octaveGrid)); 
        file.write((uint8_t*)layerNotes, sizeof(layerNotes)); 
        
        // حفظ وضعية التطبيق الحالية
        uint8_t modeToSave = (uint8_t)currentMode; 
        file.write(&modeToSave, sizeof(modeToSave));
        
        file.close(); 
    }
    
    // --- 2. معالجة وحفظ ملف الجدران الفيزياء ---
    sprintf(fileName, "W%02d.BIN", presetNum);
    if (SD.exists(fileName)) { SD.remove(fileName); } // مسح ملف الجدران القديم
    
    file = SD.open(fileName, FILE_WRITE);
    if (file) { 
        file.write((uint8_t*)walls, sizeof(walls)); 
        file.close(); 
    }
    
    EEPROM.write(50, presetNum); 
    matrixDisplay.shutdown(0, true); matrixBlinkActive = true; matrixBlinkTimer = millis();
}

void loadPresetFromSD(int presetNum) {
    if (!sdCardReady) return;
    char fileName[13]; 
    
    // --- 1. استدعاء ملف الـ Preset وقراءة كافة البايتات بالتسلسل الصارم ---
    sprintf(fileName, "P%02d.BIN", presetNum);
    if (SD.exists(fileName)) {
        File file = SD.open(fileName, FILE_READ);
        if (file) { 
            file.read((uint8_t*)gridRB, sizeof(gridRB)); 
            file.read((uint8_t*)velocityGrid, sizeof(velocityGrid)); 
            file.read((uint8_t*)octaveGrid, sizeof(octaveGrid)); 
            file.read((uint8_t*)layerNotes, sizeof(layerNotes)); 
            
            // 🌟 استرجاع البايت الأخير الخاص بوضعية التطبيق (AppMode) لمنع تعليق المؤشر
            if (file.available()) {
                uint8_t loadedMode;
                file.read(&loadedMode, sizeof(loadedMode));
                if (loadedMode >= MAIN_MENU && loadedMode <= SAMPLER_MODE) {
                    currentMode = (AppMode)loadedMode;
                }
            }
            file.close(); 
        }
    }
    
    // --- 2. استدعاء خريطة الجدران وتحديثها حياً ---
    sprintf(fileName, "W%02d.BIN", presetNum);
    if (SD.exists(fileName)) {
        File file = SD.open(fileName, FILE_READ);
        if (file) { 
            file.read((uint8_t*)walls, sizeof(walls)); 
            file.close(); 
        }
        updateWallMap(); // تفعيل الحسابات الفيزيائية حياً فور الاستدعاء
    }
}
*/
void savePresetToSD(int presetNum) {
    if (!sdCardReady) return;
    char fileName[13]; 
    
    // --- 1. معالجة وحفظ ملف الـ Preset الأساسي ---
    sprintf(fileName, "P%02d.BIN", presetNum);
    if (SD.exists(fileName)) { SD.remove(fileName); } 
    
    File file = SD.open(fileName, FILE_WRITE);
    if (file) { 
        file.write((uint8_t*)gridRB, sizeof(gridRB)); 
        file.write((uint8_t*)velocityGrid, sizeof(velocityGrid)); 
        file.write((uint8_t*)octaveGrid, sizeof(octaveGrid)); 
        file.write((uint8_t*)layerNotes, sizeof(layerNotes)); 
        
        // البيانات الجديدة
        file.write((uint8_t*)samplerNotes, sizeof(samplerNotes)); 
        
        uint8_t loopIdxByte = (uint8_t)activeLoopIndex;
        file.write(&loopIdxByte, 1); 
        
        uint16_t filterHzWord = (uint16_t)loopCutoffHz;
        file.write((uint8_t*)&filterHzWord, 2); 
        
        uint8_t modeToSave = (uint8_t)currentMode; 
        file.write(&modeToSave, 1);
        
        file.close(); 
    }
    
    // --- 2. معالجة وحفظ ملف الجدران الفيزياء ---
    sprintf(fileName, "W%02d.BIN", presetNum);
    if (SD.exists(fileName)) { SD.remove(fileName); }
    file = SD.open(fileName, FILE_WRITE);
    if (file) { 
        file.write((uint8_t*)walls, sizeof(walls)); 
        file.close(); 
    }
    
    EEPROM.write(50, presetNum); 
    matrixDisplay.shutdown(0, true); matrixBlinkActive = true; matrixBlinkTimer = millis();
}

void loadPresetFromSD(int presetNum) {
    if (!sdCardReady) return;
    char fileName[13]; 
    
    sprintf(fileName, "P%02d.BIN", presetNum);
    if (SD.exists(fileName)) {
        File file = SD.open(fileName, FILE_READ);
        if (file) { 
            uint32_t fileSize = file.size();
            // حساب الحجم القديم الأدنى (مصفوفات السيكوينسر الأساسية فقط)
            uint32_t minRequiredSize = sizeof(gridRB) + sizeof(velocityGrid) + sizeof(octaveGrid) + sizeof(layerNotes);
            
            // صمام أمان فولاذي: إذا كان الملف تالفاً أو حجمه أصغر من المتوقع، نغلقه فوراً لمنع الـ Crash
            if (fileSize < minRequiredSize) {
                Serial.println("Warning: Old or invalid preset file size. Skipping safely.");
                file.close();
                return; 
            }

            // قراءة البيانات الأساسية الآمنة لجميع الملفات
            file.read((uint8_t*)gridRB, sizeof(gridRB)); 
            file.read((uint8_t*)velocityGrid, sizeof(velocityGrid)); 
            file.read((uint8_t*)octaveGrid, sizeof(octaveGrid)); 
            file.read((uint8_t*)layerNotes, sizeof(layerNotes)); 
            
            // 🌟 فحص ذكي: لا نقرأ السامبلر واللوبر إلا إذا كان الملف جديداً ويحتوي على مساحة كافية لها
            if (file.available() >= sizeof(samplerNotes)) {
                file.read((uint8_t*)samplerNotes, sizeof(samplerNotes));
                for(int i=0; i<8; i++) { samplerNotes[i] = constrain(samplerNotes[i], 0, 20); }
            }
            
            if (file.available() >= 1) {
                uint8_t loopIdxByte;
                file.read(&loopIdxByte, 1);
                activeLoopIndex = constrain((int)loopIdxByte, 0, 3);
            }
            
            if (file.available() >= 2) {
                uint16_t filterHzWord;
                file.read((uint8_t*)&filterHzWord, 2);
                loopCutoffHz = (int)filterHzWord;
                lowPassFilter.frequency(loopCutoffHz);
            }
            
            if (file.available() >= 1) {
                uint8_t loadedMode;
                file.read(&loadedMode, 1);
                if (loadedMode >= MAIN_MENU && loadedMode <= LOOPER_MODE) {
                    currentMode = (AppMode)loadedMode;
                }
            }
            file.close(); 
        }
    }
    
    // --- 2. استدعاء خريطة الجدران وتحديثها حياً ---
    sprintf(fileName, "W%02d.BIN", presetNum);
    if (SD.exists(fileName)) {
        File file = SD.open(fileName, FILE_READ);
        if (file) { 
            file.read((uint8_t*)walls, sizeof(walls)); 
            file.close(); 
        }
        updateWallMap();
    }
}



void drawLetter(char l, int sX, int sY, CRGB c, CRGB* tL) {
    if (sX < 0 || sX >= MW || sY < 0 || sY >= MH) return;
    if (l == 'M') { for (int y = 0; y < 5; y++) { if (sX + 4 < MW) { tL[getMatrixXY(sX, sY + y)] = c; tL[getMatrixXY(sX + 4, sY + y)] = c; } } if (sX + 3 < MW) { tL[getMatrixXY(sX + 1, sY + 1)] = c; tL[getMatrixXY(sX + 2, sY + 2)] = c; tL[getMatrixXY(sX + 3, sY + 1)] = c; } }
    else if (l == 'U') { for (int y = 0; y < 5; y++) { if (sX + 3 < MW) { tL[getMatrixXY(sX, sY + y)] = c; tL[getMatrixXY(sX + 3, sY + y)] = c; } } for (int x = 0; x < 4; x++) { if (sX + x < MW) tL[getMatrixXY(sX + x, sY + 4)] = c; } }
    else if (l == 'S') { for (int x = 0; x < 4; x++) { if (sX + x < MW) { tL[getMatrixXY(sX + x, sY)] = c; tL[getMatrixXY(sX + x, sY + 2)] = c; tL[getMatrixXY(sX + x, sY + 4)] = c; } } tL[getMatrixXY(sX, sY + 1)] = c; if (sX + 3 < MW) tL[getMatrixXY(sX + 3, sY + 3)] = c; }
    else if (l == 'I') { for (int y = 0; y < 5; y++) { if (sX + 1 < MW) tL[getMatrixXY(sX + 1, sY + y)] = c; } for (int x = 0; x < 3; x++) { if (sX + x < MW) { tL[getMatrixXY(sX + x, sY)] = c; tL[getMatrixXY(sX + x, sY + 4)] = c; } } }
    else if (l == 'C') { for (int y = 0; y < 5; y++) { tL[getMatrixXY(sX, sY + y)] = c; } for (int x = 0; x < 4; x++) { if (sX + x < MW) { tL[getMatrixXY(sX + x, sY)] = c; tL[getMatrixXY(sX + x, sY + 4)] = c; } } }
    else if (l == 'B') { for (int y = 0; y < 5; y++) tL[getMatrixXY(sX, sY + y)] = c; for (int x = 0; x < 3; x++) { if (sX + x < MW) { tL[getMatrixXY(sX + x, sY)] = c; tL[getMatrixXY(sX + x, sY + 2)] = c; tL[getMatrixXY(sX + x, sY + 4)] = c; } } if (sX + 3 < MW) { tL[getMatrixXY(sX + 3, sY + 1)] = c; tL[getMatrixXY(sX + 3, sY + 3)] = c; } }
    else if (l == 'O') { for (int y = 0; y < 5; y++) { if (sX + 3 < MW) { tL[getMatrixXY(sX, sY + y)] = c; tL[getMatrixXY(sX + 3, sY + y)] = c; } } for (int x = 0; x < 4; x++) { if (sX + x < MW) { tL[getMatrixXY(sX + x, sY)] = c; tL[getMatrixXY(sX + x, sY + 4)] = c; } } }
    else if (l == 'X') { for (int i = 0; i < 5; i++) { if (sX + i < MW && sX + 4 - i < MW) { tL[getMatrixXY(sX + i, sY + i)] = c; tL[getMatrixXY(sX + 4 - i, sY + i)] = c; } } }
}

void midiSend(byte c, byte d1, byte d2) { 
    Serial1.write(c); Serial1.write(d1); 
    if((c&0xF0)!=0xC0) Serial1.write(d2); 
    byte cmdType = c & 0xF0; byte ch = (c & 0x0F) + 1;
    if (cmdType == 0x90) usbMIDI.sendNoteOn(d1, d2, ch); 
    else if (cmdType == 0x80) usbMIDI.sendNoteOff(d1, d2, ch); 
}

void sendInstrumentChange(byte channel, byte patchNum) { midiSend(0xC0 | channel, patchNum, 0); }
void allNotesOff() { 
    for(int i=0; i<8; i++) midiSend(0x80 | rhythmChannel, layerNotes[i], 0);
}


