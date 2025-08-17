# Random Audio Player with ATTiny13A and DFPlayer

This project provides a simple, low-power solution for a button-activated random audio player. It uses an **ATTiny13A microcontroller** to control a **DFPlayer Mini** module, allowing it to play a randomly selected audio file from an SD card each time a button is pressed. The system is designed to be highly power-efficient, entering a deep sleep mode after a period of inactivity and waking up instantly when the button is pressed.

---

## 🚀 Features

* **Button-Activated Playback:** Pressing a button triggers the playback of a random audio file.
* **Weighted Random Playback:** Configurable weighting to favor one folder over another (default: Folder 1 has a 90% chance of being selected).
* **Low-Power Deep Sleep:** The ATTiny13A and DFPlayer are powered down after a configurable idle period to conserve battery.
* **Instant Wake-Up:** A pin-change interrupt on the button pin allows for immediate wake-up from sleep mode.
* **Simple Hardware:** Designed for a minimal component count, making it ideal for small, embedded projects.

---

## 🛠️ Hardware Requirements

* **Microcontroller:** ATTiny13A
* **Audio Module:** DFPlayer Mini
* **Button:** A momentary push button
* **Power Mosfet:** To control the power supply to the DFPlayer.
* **Resistors:** ~1kΩ for the DFPlayer's serial line.
* **Power Source:** 5V
* **SD Card:** Formatted with audio files in folders named `01`, `02`, etc.

---

## 📌 Pinout

* **PIN2 (PB3):** Button input (with internal pull-up enabled).
* **PIN3 (PB4):** Serial TX to the DFPlayer's RX pin.
* **PIN6 (PB1):** Output to control the power mosfet for the DFPlayer.
* **PIN4 (GND):** Ground.
* **PIN8 (VCC):** 5V power supply.

---

## ⚙️ Configuration

The project can be configured by editing the `#define` values at the top of the `main.cpp` file.

```c++
// -------- User settings --------
#define FOLDER1        0x01
#define FILE1_MIN      1
#define FILE1_MAX      27
#define FOLDER2        0x02
#define FILE2_MIN      1
#define FILE2_MAX      43
#define WEIGHT_FOLDER1 90          // % chance for folder 1
#define SLEEP_TIMEOUT  60000       // 60 seconds