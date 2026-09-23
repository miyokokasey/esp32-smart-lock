# ESP32 Smart Lock

A dual-authentication smart lock system built with an ESP32 microcontroller,
supporting RFID card/fob access, Bluetooth mobile unlock, and Wi-Fi web unlock.
Engineered with an isolated power rail to prevent MCU brownouts during solenoid
activation.

---

## Overview

This project implements a fully functional smart lock system capable of authenticating users through two independent methods: physical RFID card/fob access via an RC522 reader, and an HTTP web interface served directly by the ESP32 over Wi-Fi — accessible from any browser on the same network. The system was designed around real hardware constraints encountered during development, most notably the solenoid inrush current problem that required isolating the lock's power rail from the microcontroller supply.

The build surfaced and resolved several non-trivial hardware and firmware challenges, each documented below as engineering decisions rather than just implementation notes.

---

## Features

- **Dual authentication** — RFID (RC522 / ISO 14443A) and Wi-Fi HTTP server operate concurrently in a single firmware image
- **Isolated 12V power rail** — dedicated supply for solenoid and relay prevents ESP32 brownouts from inrush current spikes during actuation
- **Flyback protection** — 1N4007 diode across solenoid coil clamps inductive kickback voltage spike on relay open
- **Active-high relay logic** — corrected from default assumption after observing reversed lock behavior; solenoid wired to NO terminal with HIGH = unlock
- **Multi-card whitelist** — UID array supports N authorized cards; checked via O(n) linear scan per read cycle
- **Passive buzzer audio feedback** — distinct tones (1000 Hz unlock / 400 Hz deny) via hardware PWM `tone()` on GPIO 27
- **Graceful Wi-Fi fallback** — if Wi-Fi credentials fail or network is unavailable, RFID authentication remains fully operational independently
- **Flash partition optimization** — firmware image exceeds default 1.3 MB limit; resolved by switching to Huge APP (3MB No OTA) partition scheme

---

## Hardware

| Component | Part / Spec | Notes |
|---|---|---|
| Microcontroller | DOIT ESP32 DevKit V1 (ESP32-WROOM-32) | Dual-core 240 MHz, 4MB flash, built-in Wi-Fi |
| RFID reader | RC522 (MFRC522) — 13.56 MHz, SPI | Power pin labeled `3.3V` on this board variant — not `VCC` |
| Relay module | 5V, 1-channel, optocoupler-isolated | Active-HIGH on this module; confirmed by behavioral testing |
| Solenoid lock | 12V DC, normally-closed | NC = bolt extended (locked) when unpowered — safe-fail default |
| Power supply | 12V 2A DC wall adapter, 5.5mm barrel jack | Dedicated rail for solenoid — isolated from ESP32 USB supply |
| Barrel jack breakout | 5.5mm × 2.1mm screw terminal adapter | Center pin = positive (+) — verified with multimeter |
| Flyback diode | 1N4007 | Across solenoid terminals; cathode (banded end) to positive wire |
| Green LED | 5mm, ~2.1V forward voltage | GPIO 25 via 220Ω resistor |
| Red LED | 5mm, ~2.1V forward voltage | GPIO 26 via 220Ω resistor |
| Passive buzzer | 5V piezo | GPIO 27 — `tone()` controls pitch; 1000 Hz grant / 400 Hz deny |

**Estimated build cost: ~$35–45 USD**

---

## Pin Assignments

```
ESP32 GPIO    Component            Notes
──────────────────────────────────────────────────────
GPIO 5        RC522 SDA (SS)       SPI chip select — active low
GPIO 18       RC522 SCK            Hardware VSPI clock
GPIO 19       RC522 MISO           SPI data in to ESP32
GPIO 22       RC522 RST            Reset line — driven by MFRC522 library
GPIO 23       RC522 MOSI           SPI data out from ESP32
3V3           RC522 3.3V           3.3V ONLY — 5V will destroy the module
GND           RC522 GND            Common ground

GPIO 32       Relay IN             Active-HIGH on this module
Vin (5V)      Relay VCC            USB 5V passthrough — relay coil needs 5V
GND           Relay GND            Shared ground

GPIO 25       Green LED            Via 220Ω resistor — access granted
GPIO 26       Red LED              Via 220Ω resistor — access denied
GPIO 27       Passive buzzer (+)   PWM tone output
```

---

## Wiring Diagram

### 3.3V circuit — ESP32 ↔ RC522 (SPI)

```
ESP32                    RC522
─────                    ─────
3V3      ────────────►   3.3V      ← STRICTLY 3.3V — never 5V
GND      ────────────►   GND
GPIO 22  ────────────►   RST
GPIO 5   ────────────►   SDA (SS)
GPIO 18  ────────────►   SCK
GPIO 23  ────────────►   MOSI
GPIO 19  ◄────────────   MISO
                         IRQ       ← not connected
```

### 5V circuit — ESP32 → relay module

```
ESP32 Vin (5V)  ────────►  Relay VCC
ESP32 GND       ────────►  Relay GND
GPIO 32         ────────►  Relay IN     ← HIGH = relay ON (active-high)
```

### 12V circuit — relay → solenoid (isolated rail)

```
[12V adapter +] → barrel jack (+) → Relay COM
                                          │
                                     Relay NO ──► Solenoid (+) red wire
                                                          │
                               1N4007 diode (cathode/band → this side)
                                                          │
[12V adapter −] → barrel jack (−) → Shared GND ◄── Solenoid (−) black wire
                                          ↑
                                    ESP32 GND also ties here
```

> **Critical:** Both the 12V adapter negative and ESP32 GND must share one common ground rail. Without this shared reference the solenoid circuit has no return path.

---

## Software

### Dependencies

| Library | Source | Purpose |
|---|---|---|
| `SPI.h` | ESP32 Arduino core (built-in) | SPI hardware peripheral for RC522 |
| `MFRC522.h` | GithubCommunity / miguelbalboa — Library Manager | RC522 register-level abstraction |
| `WiFi.h` | ESP32 Arduino core (built-in) | 802.11 b/g/n station mode |
| `WebServer.h` | ESP32 Arduino core (built-in) | HTTP server on port 80 |

### Arduino IDE Setup

1. **Add ESP32 board URL** — File → Preferences → Additional boards manager URLs:
   ```
   https://raw.githubusercontent.com/espressif/arduino-esp32/gh-pages/package_esp32_index.json
   ```

2. **Install board package** — Tools → Board Manager → search `esp32` → install **esp32 by Espressif Systems**

3. **Install MFRC522** — Sketch → Include Library → Manage Libraries → search `MFRC522` → install by GithubCommunity

4. **Board selection** — Tools → Board → **ESP32 Dev Module**
   > Note: Select `ESP32 Dev Module`, not `DOIT ESP32 DEVKIT V1`. The DevKit V1 profile hides the Partition Scheme menu. Both profiles target identical hardware — the board selection only affects available IDE options.

5. **Partition scheme** — Tools → Partition Scheme → **Huge APP (3MB No OTA)**
   > Required: the combined Wi-Fi + RFID firmware exceeds the default 1.3 MB flash code partition. Huge APP expands it to ~3 MB within the same 4 MB flash chip — no hardware change needed.

6. **Upload note** — if upload fails with `Wrong boot mode detected`, hold the **BOOT** button on the ESP32 while clicking Upload, release when the progress percentage begins incrementing.

### Configuration

Before uploading, update these two items in the sketch:

```cpp
// Wi-Fi credentials
const char* WIFI_SSID = "YourNetworkName";
const char* WIFI_PASS = "YourPassword";

// Authorized card UIDs — scan with uid_scanner sketch first, then paste here
byte authorizedUIDs[][4] = {
  {0x75, 0x5C, 0x44, 0xE0},   // Card 1
  {0x8A, 0x51, 0xD4, 0x35}    // Card 2 / key fob
};
```

To read a card's UID: upload `src/uid_scanner/uid_scanner.ino`, open Serial Monitor at **115200 baud**, and tap the card. The 4 hex bytes are printed directly — prefix each with `0x` when entering into the array.

---

## Authentication Flow

```
Card tap ───┐
            ├──► Credential check
HTTP /unlock ┘         │
                ┌──────┴───────┐
              match         no match
                │               │
         GPIO 32 HIGH    GPIO 32 stays LOW
         Relay closes    Red LED on
         12V → solenoid  Low tone (400 Hz, 400 ms)
         Green LED on    1 second delay, LED off
         High tone
         (1000 Hz, 200 ms)
         3 second timer
         GPIO 32 LOW
         Relay opens
         Bolt re-engages
```

---

## Engineering Challenges and Resolutions

### 1. ESP32 brownout on relay activation

**Problem:** The ESP32 reset randomly every time the solenoid was triggered. Serial Monitor disconnected mid-operation with no error message.

**Root cause:** The relay coil's inrush current (~600 mA peak) caused a voltage droop on the shared supply rail, dropping VDD below the ESP32's hardware brownout threshold (~2.44 V). The brownout detector triggered a hard reset before the unlock sequence could complete.

**Resolution:** Separated the solenoid and relay onto a dedicated 12V wall adapter rail, completely isolated from the ESP32's USB/3.3V supply. The two rails share only a common GND reference. The ESP32 GPIO only drives the relay module's optocoupler — a microamp-level signal — not the coil current. A 1N4007 flyback diode across the solenoid terminals clamps the inductive voltage spike when the relay de-energizes.

---

### 2. Relay logic inversion

**Problem:** On first upload, the solenoid retracted continuously at idle and released when a valid card was scanned — completely reversed from intended behavior.

**Root cause:** The relay module uses active-HIGH logic on this specific board variant (not the more common active-LOW), and the solenoid was connected to the NO (normally open) terminal.

**Resolution:** Confirmed behavior by uploading an isolated relay test sketch. Inverted the GPIO logic in firmware: `HIGH` = relay ON = unlock, `LOW` = relay OFF = locked. The initial pin state in `setup()` is set to `LOW` before any other code runs, ensuring the door is locked on every power-up.

---

### 3. RC522 interference from conductive desk surface

**Problem:** RFID reads were intermittent or non-functional when the RC522 module rested flat on the work surface, but fully reliable when held in the air.

**Root cause:** The desk surface created capacitive loading on the antenna traces and eddy current damping of the 13.56 MHz magnetic field from the reader coil. The bare solder contacts on the module underside also created partial shorts through surface conductance, altering the antenna's resonant characteristics.

**Resolution:** Elevated the RC522 on a non-conductive standoff (cardboard during prototyping). In a production enclosure, mounting inside a plastic housing permanently isolates the underside contacts and eliminates the interference path.

---

### 4. Firmware size exceeding flash partition limit

**Problem:** Compilation error — `text section exceeds available space in board` — with the combined Wi-Fi + WebServer + MFRC522 firmware image exceeding the 1.3 MB default code partition.

**Resolution:** Changed Tools → Partition Scheme to **Huge APP (3MB No OTA)**, reallocating the 4 MB flash chip to provide ~3 MB for the code partition instead of ~1.3 MB. No hardware change required. The `Partition Scheme` menu is only visible when `ESP32 Dev Module` is selected as the board — it is hidden under the `DOIT ESP32 DEVKIT V1` profile.

---

### 5. HTML string compilation errors in web handler

**Problem:** Multi-line HTML strings inside `server.send()` caused `missing terminating " character` compiler errors when strings wrapped across lines in the IDE.

**Resolution:** Replaced concatenated C-style string literals with C++ raw string literals (`R"HTML(...)HTML"`), which treat the entire enclosed block as raw text and eliminate all per-line quotation mark requirements.

---

## Project Structure

```
esp32-smart-lock/
├── src/
│   ├── smart_lock/
│   │   └── smart_lock.ino        Main firmware — RFID + Wi-Fi
│   └── uid_scanner/
│       └── uid_scanner.ino       Utility — prints card UID to Serial Monitor
├── docs/
│   ├── wiring-diagram.md         Full wiring reference with circuit explanation
│   ├── system-architecture.md    Design decisions and state machine
│   └── images/
│       └── circuit-photo.jpg     Build photo
├── hardware/
│   └── bill-of-materials.md      Parts list with prices and search terms
├── .gitignore
├── LICENSE
└── README.md
```

---

## Usage

### RFID unlock

Hold your authorized MIFARE card or key fob flat against the RC522 antenna coil (the large square loop on the module) within ~3–5 cm. On a valid read: green LED on, 1000 Hz beep, solenoid retracts. Re-locks automatically after 3 seconds. On an invalid card: red LED on, 400 Hz tone, solenoid stays locked.

> If reads are intermittent, ensure the module is not resting on a conductive surface — elevate it on cardboard or plastic.

### Wi-Fi unlock

1. Ensure your phone and the ESP32 are on the same Wi-Fi network
2. Open Serial Monitor at 115200 baud after boot — the assigned IP prints as: `IP: 192.168.x.x`
3. Open a browser on your phone and navigate to `http://[IP-address]`
4. Tap **Unlock Door** — the page confirms unlock and the solenoid retracts

> If the page does not load, confirm cellular data is off and the phone is connected to Wi-Fi. Some routers block device-to-device communication (AP isolation) — switching to a phone hotspot works around this.

---

## License

MIT — see [LICENSE](LICENSE)

---

*Electrical Engineering — [University of California Sna Diego], [2026]*
---

## Author

[Kasey Miyoko] — [kaseymiyoko@email.com] — [www.linkedin.com/in/kasey-miyoko]
