# musicbox
teensy 4.1 steps sequencer machine
An advanced DIY hardware and software interactive audio instrument and groovebox powered by the **Teensy 4.1** and the Teensy Audio Board.
It features real-time raw audio sample looping, live time-stretching, dynamic step sequencing, and hardware filtering.

### ✨ Key Features
* **Looper Mode:** Seamless real-time playback and retriggering of raw custom audio loops (`.RAW` 16-bit mono format).
* **Live Time Stretching:** On-the-fly dynamic control over playback speed and pitch using the `TeensyVariablePlayback` engine.
* **Hardware State Variable Filter:** Real-time low-pass filter with dynamic Cutoff frequency mapping.
* **Hardware Interface Control:**
  * **Knob 1 (A0):** Controls the loop window and loop length slicing (`looperWindowMs`).
  * **Knob 2 (A1):** Controls the **Playback Rate** (Live Tempo speed & Time Stretching).
  * **Knob 3 (A2):** Controls the Filter Cutoff frequency.
* **Interactive LED Matrix:** Real-time visual tracking of the sequencer cursor and active beats using the `FastLED` library, providing dynamic, audio-reactive visual feedback.
### 💾 Requirements & Libraries
Make sure to install the following libraries in your **Arduino IDE**:
1. `TeensyVariablePlayback` (by newdigate) - For low-latency variable playback rate.
2. `FastLED` - For dynamic LED matrix visual feedback.
3. `SD` & `SdFat` - Optimized for Teensy 4.1 high-speed SD card audio streaming.

### 🚀 Hardware Configuration
* **Microcontroller:** Teensy 4.1
* **Audio Hardware:** Teensy Audio Shield
* **Inputs:** 3 Potentiometers (A0, A1, A2) + Navigation Control Buttons.
* **Hardware MIDI Synthesis (VS1053 Support):** Integrated support for the VS1053 audio decoder chip in Real-time MIDI mode, allowing hardware-based instrument generation (General MIDI bank) with zero CPU overhead on the Teensy.
