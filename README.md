# 🎛️ SC01 Plus — WipidiM Controller Firmware

<p align="center">
  <img src="Misc/Device.jpeg" width="850">
</p>

<p align="center">
High-resolution Bluetooth MIDI controller firmware for the WT32 SC01 Plus platform.<br>
Powered by 20× AS5600 magnetic encoders, LVGL touchscreen UI, BLE MIDI, Web Configuration, and onboard modulation engines.
</p>

---

# ✨ Features

## 🎚 Hardware Features

| Feature | Details |
|---|---|
| 20× AS5600 Magnetic Encoders | Connected through 3× TCA9548 I2C multiplexers |
| WT32 SC01 Plus | ESP32-S3 touchscreen platform |
| Touch Buttons | Shift + LFO dedicated hardware buttons |
| SD Card Storage | JSON-based configuration system |
| Bluetooth MIDI | BLE MIDI enabled by default |
| WiFi Web Server | Browser-based live configuration |

---

## 🎛 MIDI Features

- 8 Pages × 16 Encoders × 16 MIDI Channels
- **2048 mappable MIDI CC controls**
- MIDI CC + MIDI Note support (TODO : program change and sysex)
- BLE MIDI
- Optional USB MIDI
- Per-channel page and shortcuts recall
- Mixer mode pages
- Dynamic encoder labels, behavior, CC# and Channel#
- Real-time MIDI feedback

Built using the excellent [Control Surface Library](https://github.com/tttapa/Control-Surface).

---

## 🌊 LFO Engine
<p align="center">
  <img src="Misc/LFO_Mode.jpeg" width="850">
</p>
### 4 Independent LFOs

Each LFO supports:

- Adjustable rate
- Adjustable depth
- Offset
- Multiple waveforms
- Visual waveform preview

### Mix Mode

Combine multiple LFOs together for:

- Complex modulation
- Layered movement
- Rhythmic automation
- Evolving MIDI control
- Simple raw chaos

---

# 🎯 What Makes This Different

✅ No coding required for daily use... yet fully open source and hackable
✅ Fully editable from touchscreen, browser, or SD card  
✅ 2048 assignable MIDI parameters  
✅ Real-time modulation engine  
✅ Persistent configuration storage  
✅ Designed specifically for DAWs, synths, and live rigs with component that won't fail (use manipulates passive magnet and capacitive buttons = no mechanical flaw)

---

# 🖼 Screenshots

## Main Interface

<p align="center">
  <img src="Misc/Buttons_CCMode.jpeg" width="750">
</p>
<p align="center">
  <img src="Misc/Keyboard_Mode.jpeg" width="750">
</p>
---

## Mixer Mode

<p align="center">
  <img src="Misc/Mixer_Mode.jpeg" width="750">
</p>

---

## LFO Mode

<p align="center">
  <img src="Misc/LFO_Mode.jpeg" width="750">
</p>

---

## Web Interface

<p align="center">
  <img src="Misc/Pot_page.png" width="750">
</p>
<p align="center">
  <img src="Misc/Mixer_Page.png" width="750">
</p>
---

# 🔌 Hardware Wiring

## AS5600 Encoder Connections

we use 20 of the 24 available busses of the 3 tca9548

| AS5600 | TCA9548 / ESP32 |
|---|---|
| VCC | 3.3V |
| GND | GND |
| SDA | SC01Plus pin10 | 
| SCL | SC01Plus pin11 | 

| DIR | Not connected |
| OUT | Not connected |

---

## I2C Multiplexer Layout

The firmware supports:

- 3× TCA9548A multiplexers
- Multiple independent encoder buses
- Configurable I2C pins

Example:

```Config.h
// ========== HARDWARE PINS ==========
static const int PIN_TOUCH_1 = 12;
static const int PIN_TOUCH_2 = 14;
static const int PIN_SCL = 11;
static const int PIN_SDA = 10;

static const int PIN_SD_SCK = 39;
static const int PIN_SD_MISO = 38;
static const int PIN_SD_MOSI = 40;
static const int PIN_SD_CS = 41;

static const int PIN_VIBRATION_MOTOR = 13;
static const int VIBRATION_DURATION_MS = 5;
```

---

# 🧭 Controls & Navigation

# 🎛 Shift Button (Pin 12)

| Action | Result |
|---|---|
| Short Press | Momentary mixer mode |
| Hold + Mixer Page | Latching mixer mode |

---

# 🌊 LFO Button (Pin 14)

| Action | Result |
|---|---|
| Press | Toggle LFO mode |

---

# 🎚 Mixer Mode

## Features

- 5 Custom Mixer Pages
- Fast-access controls
- Shortcut encoder mirroring
- Instant live performance access

Perfect for:

- Volume
- FX sends
- Filter controls
- Performance macros

---

# 🎹 CC Mode

## Per-Channel Editing

Each MIDI channel stores:

- Current page
- Encoder values
- Active mappings
- Labels
- LFO routing

Supports all:

:contentReference[oaicite:0]{index=0}

---

# 🌐 Web Interface

## Browser-Based Configuration

The built-in WiFi web server allows:

- MIDI remapping
- Encoder settings
- Button note assignment
- Channel configuration
- Label editing
- LFO routing
- Live configuration updates

---

## Connecting
<p align="center">
  <img src="Misc/Edit_Mode.jpeg" width="750">
</p>
### Step 1

Tap the Performance monitor panel in the bottom left to enter Edit Mode
### Step 2

Tap the WiFi connect button.

### Step 3

The device displays its IP address.

Example:

```text
192.168.1.42
```

### Step 4

Open browser:

```text
http://192.168.1.42
```

---

# 💾 SD Card Configuration
<p align="center">
  <img src="Misc/File_page.png" width="750">
</p>
## Editable JSON Files

### midi_config.json

Stores:

- MIDI CC mappings
- Channels
- Notes
- LFO assignments
- Mixer pages

---

### arc_labels.json

Stores:

- Encoder labels
- Page names
- UI labels

---

# 📂 SD Card Structure
SD Card file are manageable in the Web Interface in the SD File Tab

```text
sd_card/
├── midi_config.json
└── arc_labels.json
```

---

# 🔧 Customization

## No Recompilation Required

You can fully customize the controller through:

- Touchscreen UI
- Web interface
- Direct SD card editing

---

# 🚀 Getting Started

# Hardware Requirements

- WT32 SC01 Plus
- 20× AS5600 encoders
- 3× TCA9548A multiplexers
- SD card

---

# 📡 Bluetooth MIDI

BLE MIDI is enabled by default.

Compatibility and fun garanteed and tested in :

- DAWs (Ableton, Logic, Reaper)
- Mobile Apps (drambo, gadget)
- Hardware synths

---

# 🖥 USB MIDI

USB MIDI is optional (TODO)

Enable manually in build configuration.

Example:

```cpp
#define ENABLE_USB_MIDI
```

---

# 📜 Dependencies

| Library | Purpose |
|---|---|
| Control Surface | MIDI engine |
| LVGL | GUI rendering |
| ArduinoJson | Config parsing |
| Wire | I2C communication |

---

# 🎨 UI System

UI created using:

- SquareLine Studio
you can modify sizes color and disposition of objects but removing an object or changing its name may prevent compilation.

Features:

- Touch interaction
- Animated widgets
- Real-time updates
- Waveform displays
- Responsive layouts

---

# ⚠️ Known Limitations

A few bugs to check : 
-shortcut pots miroring value but not pot Style when symmetrical
-we cannot say the build is complicated but it gets messy easily
<p align="center">
  <img src="Misc/Ghetto_solder.jpeg" width="850">
</p>
this ghetto prototype also has usbc port for the firmware upload and charging the battery thru a tp4056 and an scrapped iPhone 5c battery
i thought the vibration motor would be a nice addition for haptic feedback when tweaking symmetrical knobs and "feel" the detent but using a quiet device is even better, might get removed
---

# 🛠 Planned Features

- MIDI clock sync
- Additional LFO waveforms
- Full web and onboard MIDI editing
- OSC support
- program change and system exclusive messages support
- MIDI learn mode
- SD preset manager
- ws2812 support
- mpr121 support
- mcp23017 support
- Multi-device sync
- CV/Gate i/o module extension
- Complex chord mode with progressions and parallel sending
- Hardware additionnal keyboard with Ws2812 and MPR121
- midi DIN in/out/thru
- more embedded LFOs
- modular menu system
- add board définitions to bring WidipiM to other hardware (teensy,esp32... with generic screens)
- real pcb with led rings for each encoder
- make it waterproof ?

---

# 🤝 Contributing

Pull requests are welcome.
i´m not a coder IRL so everything in this repo is mainly (subscription free) AI slop and an insane amount of testing and debugging. 
what do you use and what do you want ? 
Ideas, improvements, and feature requests are encouraged.
monsieurgrosconnard(at)gmail.com
---

# 📜 License

MIT License

---

# ❤️ Credits

## Libraries

- LVGL
- Control Surface
- SquareLine Studio

---

