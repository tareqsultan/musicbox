# musicbox
teensy 4.1 steps sequencer machine
An advanced DIY hardware and software interactive audio instrument and groovebox powered by the **Teensy 4.1** and the Teensy Audio Board.
It features real-time raw audio sample looping, live time-stretching, dynamic step sequencing, and hardware filtering.

### ✨ Key Features
## 🕹️ Operational Modes (System Menu)

The Music Box features 5 distinct real-time operational modes, selectable via the central OLED menu and indicated by the lateral hardware LEDs:

1. **Melodic Mode (MIDI):** Utilizes the VS1053B synthesis engine to play polyphonic instrument tracks, converting step-sequencer data into live MIDI notes with selectable instruments from the General MIDI bank.

2. **Rhythm Loop Mode:** Streams pre-loaded percussion tracks and authentic Gulf folk rhythm loops directly from the SD card, keeping the tempo strictly locked with the visual LED matrix cursor.

3. **Bouncing Ball Mode:** A physics-based generative audio engine. Visual "balls" bounce across the LED matrix, triggering distinct musical notes or samples upon colliding with boundaries or specific steps.

4. **Teensy Samples Mode (RYTM Sampler):** A responsive, low-latency sampler mode utilizing internal Teensy architecture to trigger, slice, and manipulate individual acoustic drum hits and custom audio shots on the fly.

5. **Looper Mode:** The advanced audio performance mode utilizing the `TeensyVariablePlayback` engine. It reads headerless `.RAW` files.
   
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
