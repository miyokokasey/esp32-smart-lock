# ESP32 Smart Lock

A dual-authentication smart lock system built with an ESP32 microcontroller,
supporting RFID card/fob access, Bluetooth mobile unlock, and Wi-Fi web unlock.
Engineered with an isolated power rail to prevent MCU brownouts during solenoid
activation.

![Demo](docs/images/circuit-photo.jpg)

---

## Features

- **RFID authentication** — 13.56 MHz MIFARE cards and key fobs via RC522 module
- **Bluetooth unlock** — Classic BT SPP; pair phone and send `UNLOCK` command
- **Wi-Fi unlock** — ESP32 hosts HTTP server; visit `/unlock` from any browser
  on the same network
- **Isolated power rail** — Dedicated 12V supply for solenoid/relay prevents
  ESP32 brownouts from inrush current spikes
- **Flyback protection** — 1N4007 diode across solenoid coil clamps inductive
  kickback voltage
- **Visual + audio feedback** — Green/red LEDs and buzzer tones for
  granted/denied access
- **Multi-card whitelist** — Supports multiple authorized UIDs stored in firmware

---
## Hardware

| Component | Spec | Purpose |
|---|---|---|
| ESP32 DevKit V1 | 240 MHz dual-core, 4MB flash | Main controller |
| RC522 RFID module | 13.56 MHz, SPI, ISO 14443A | Card reader |
| 5V relay module | Optocoupler-isolated | Switches 12V solenoid circuit |
| 12V solenoid lock | NC (normally closed) | Physical bolt |
| 12V 2A DC adapter | 5.5mm barrel jack | Dedicated lock power rail |
| 1N4007 diode | 1A, 1000V | Flyback protection |
| Green + red LEDs | 5mm, with 220Ω resistors | Access feedback |
| Active buzzer | 5V | Audio feedback |

**Estimated total cost: ~$35–45**

---

## Wiring

### ESP32 → RC522 (SPI — VSPI peripheral)

| RC522 pin | ESP32 GPIO | Notes |
|---|---|---|
| VCC | 3.3V | **3.3V only — never 5V** |
| GND | GND | Common ground |
| RST | GPIO 22 | Reset line |
| SDA (SS) | GPIO 5 | SPI chip select |
| SCK | GPIO 18 | Hardware VSPI clock |
| MOSI | GPIO 23 | SPI data out |
| MISO | GPIO 19 | SPI data in |

### ESP32 → relay module

| Relay pin | Connect to | Notes |
|---|---|---|
| VCC | ESP32 Vin (5V) | Relay coil power |
| GND | ESP32 GND | Shared ground |
| IN | GPIO 32 | Control signal (active low) |

### Relay → solenoid (12V circuit)

| Relay terminal | Connect to |
|---|---|
| COM | 12V adapter + (via barrel jack breakout) |
| NO | Solenoid + wire |
| Solenoid − | 12V adapter GND (tied to ESP32 GND) |

> **Important:** The 12V adapter GND and ESP32 USB GND must be tied together
> at a shared ground point. Without a common ground, the solenoid circuit has
> no return path.

### LEDs and buzzer
| Component | GPIO | Notes |
|---|---|---|
| Green LED | GPIO 25 | Via 220Ω resistor |
| Red LED | GPIO 26 | Via 220Ω resistor |
| Buzzer (+) | GPIO 27 | Active buzzer |

---

## Software setup

### Prerequisites

- [Arduino IDE 2.x](https://www.arduino.cc/en/software)
- ESP32 board support package (Espressif)
- MFRC522 library (GithubCommunity)

### Install ESP32 board support

1. Open Arduino IDE → File → Preferences
2. Add to "Additional boards manager URLs":
   ```
      https://raw.githubusercontent.com/espressif/arduino-esp32/gh-pages/package_esp32_index.json
   ```
3. Tools → Board Manager → search "esp32" → install **esp32 by Espressif**

### Install MFRC522 library

Sketch → Include Library → Manage Libraries → search **MFRC522** → install
(by GithubCommunity)

### Upload

1. Select board: **ESP32 Dev Module**
2. Select correct COM port
3. Open `src/smart_lock/smart_lock.ino`
4. Edit lines 16–17 with your Wi-Fi credentials
5. Upload — hold the BOOT button if the upload fails to connect

### Get your card UIDs

Before setting authorized cards, run the UID scanner sketch in
`src/smart_lock/smart_lock.ino` with the `UID_SCAN_ONLY` flag set to `true`.
Open Serial Monitor at **115200 baud**, tap your cards, and note the hex UIDs
printed. Then update the `authorizedUIDs` array with your values and re-upload.
---

## How it works

### Authentication flow

```
Card tap / BT command / HTTP request
          ↓
    Credential check
     ↙         ↘
  Match       No match
    ↓             ↓
GPIO 32 LOW   GPIO 32 stays HIGH
Relay closes  Red LED + low buzzer
12V → solenoid
Green LED + high beep
3 second timer
GPIO 32 HIGH
Relay opens
Bolt re-engages
```

### Power rail isolation (brownout prevention)
The solenoid's inrush current (~600mA peak) would drop a shared supply below
the ESP32's brownout threshold (~2.44V), causing a hardware reset mid-operation.

**Solution:** Two completely separate supply rails:
- **ESP32 rail:** USB 5V → on-board LDO → 3.3V (logic and RC522)
- **Lock rail:** 12V wall adapter → relay NO contact → solenoid

These rails share only a common GND reference. The ESP32 GPIO only drives
the relay's optocoupler (microamps), not the coil current.

### SPI communication (ESP32 ↔ RC522)

The RC522 uses SPI (Serial Peripheral Interface) — a synchronous full-duplex
4-wire protocol. The ESP32 is master; the RC522 is the slave. The hardware VSPI
peripheral handles clock generation and data framing automatically once
`SPI.begin()` is called.

---

## Project structure

```
esp32-smart-lock/
├── src/smart_lock/smart_lock.ino   Main Arduino sketch
├── docs/
│   ├── wiring-diagram.md           Detailed wiring reference
│   └── system-architecture.md      System design explanation
├── hardware/
│   └── bill-of-materials.md        Parts list with prices
└── README.md
```

---

## Known limitations and future improvements

- **UID-only RFID auth** is cloneable with cheap card writers. Production
  systems should use MIFARE DESFire with AES-128 challenge-response.
- **Plain HTTP** for the web interface. Production would use HTTPS with TLS.
- **Blocking `delay()`** in unlock function pauses all processing for 3s.
  Replace with `millis()`-based non-blocking timing for production.
- **Credentials in firmware** — Wi-Fi password is hardcoded. Should use
  ESP32 NVS (non-volatile storage) or a provisioning flow.

---

## License

MIT — see [LICENSE](LICENSE)

---

## Author

[Kasey Miyoko] — [kaseymiyoko@email.com] — [linkedin.com/in/kasey-miyoko]
