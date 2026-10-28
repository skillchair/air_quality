# Air Quality Monitor with Light Indication and Automatic Ventilation

A Raspberry Pi 4 B based measurement system that reads air quality with an **MQ-2 gas/smoke sensor**, shows the result on a **16×2 LCD**, signals the level with **green / yellow / red LEDs**, and, when the air is bad, triggers a **buzzer** and switches on a **DC fan** to ventilate the room.

The code is written in **C** and uses the Linux **I²C** subsystem (`/dev/i2c-1`) together with the **WiringPi** library.

> Seminar paper, Faculty of Electronic Engineering, University of Niš
> Course: Measurement Systems Based on Microcomputers
> Student: Andrija Grbušić
> Mentor: Prof. Dr. Jelena R. Jovanović
> Niš, June 2026

<!-- Add a photo of the finished project here, e.g.: -->
<!-- ![Finished project](images/project.jpg) -->

---

## Table of Contents

1. [How it works](#how-it-works)
2. [Behaviour / thresholds](#behaviour--thresholds)
3. [Hardware](#hardware)
4. [Wiring](#wiring)
5. [LED driver board](#led-driver-board)
6. [Software setup](#software-setup)
7. [Build and run](#build-and-run)
8. [Known issues and notes](#known-issues-and-notes)
9. [Ideas for future work](#ideas-for-future-work)
10. [References](#references)

---

## How it works

```
MQ-2 sensor ──(analog A0)──► PCF8591 ADC ──(I²C)──► Raspberry Pi 4 B
                                                         │
                          ┌──────────────┬───────────────┼───────────────┐
                          ▼              ▼               ▼               ▼
                   16×2 LCD (value)  3 LEDs (via     Active buzzer   DC motor
                                     BC547B drivers)                 (fan)
```

1. The MQ-2 sensor (a metal-oxide semiconductor / chemiresistor) lowers its resistance as the concentration of smoke and combustible gases rises, which raises the voltage on its analog output.
2. The PCF8591 converts that voltage to an 8-bit value (0–255) and sends it to the Raspberry Pi over I²C.
3. The program on the Pi compares the value to two thresholds, then sets the LEDs, buzzer and fan accordingly (a simple feedback loop) and prints the value on the LCD.

> **Note:** the MQ-2 is a multi-gas sensor. It cannot tell *which* gas is present or give an absolute concentration. It is suited to detecting general changes in air quality.

## Behaviour / thresholds

| ADC value | State | LED | Buzzer | Fan |
|-----------|-------|-----|--------|-----|
| `<= 45`   | Clean air | Green  | Off | Off |
| `46 – 55` | Warning   | Yellow | Off | Off |
| `>= 56`   | Danger    | Red    | **On** | **On** |

The system drops back to a lower level automatically as soon as the value falls below the corresponding threshold. The raw value is also printed to the terminal for debugging, and shown on the second row of the LCD.

## Hardware

- Raspberry Pi 4 B + GPIO extension board + proto (bread) board
- MQ-2 gas sensor module
- PCF8591 8-bit A/D and D/A converter module
- Active buzzer (**active-low** in this build)
- 16×2 LCD (4-bit mode)
- 10 kΩ potentiometer (LCD contrast)
- Green, yellow and red LEDs
- 3 × 10 kΩ resistors (transistor base resistors)
- 3 × 1 kΩ resistors (LED current-limiting resistors)
- 3 × BC547B NPN transistors
- DFRobot Gravity 130 DC motor module with fan (built-in transistor driver, 5 V)
- Small perfboard, soldering iron and solder wire (for the LED driver board)
- Jumper wires (M-F and M-M)
- Optional: keyboard, mouse and monitor (SSH also works if enabled in Raspberry Pi Imager)

## Wiring

Physical pin numbers refer to the 40-pin Raspberry Pi header. The WiringPi numbering used in the code is different from both the physical and the BCM/GPIO numbering. Run `gpio readall` to see all three side by side.

### PCF8591 (ADC)

| PCF8591 | Raspberry Pi | Note |
|---------|--------------|------|
| VCC | Pin 4 | 5 V |
| GND | Pin 6 | GND |
| SCL | Pin 5 | GPIO 3 (I²C SCL, WiringPi 9) |
| SDA | Pin 3 | GPIO 2 (I²C SDA, WiringPi 8) |

I²C address is `0x48` (A0, A1, A2 tied to GND).

### MQ-2 module

| MQ-2 | Connects to | Note |
|------|-------------|------|
| VCC | Pi Pin 4 | 5 V |
| GND | Pi Pin 6 | GND |
| A0 | PCF8591 AIN0 | Analog input |

### 16×2 LCD (4-bit mode)

| LCD | Raspberry Pi | WiringPi |
|-----|--------------|----------|
| VSS | Pin 6 (GND) | – |
| VDD | Pin 4 (5 V) | – |
| V0 | Potentiometer wiper (contrast) | – |
| RS | Pin 37 | 25 |
| R/W | Pin 6 (GND) | – |
| E | Pin 35 | 24 |
| D0–D3 | not connected | – |
| D4 | Pin 33 | 23 |
| D5 | Pin 31 | 22 |
| D6 | Pin 29 | 21 |
| D7 | Pin 23 | 14 |
| A (LED+) | Pin 4 (5 V) | – |
| K (LED−) | Pin 6 (GND) | – |

The 10 kΩ potentiometer has VCC on Pin 4, GND on Pin 6, and its wiper goes to the LCD `V0` pin.

### Buzzer, LED driver board and fan

| Component | Pin | Raspberry Pi pin | GPIO | WiringPi |
|-----------|-----|------------------|------|----------|
| Active buzzer | `+` | Pin 4 (5 V) | – | – |
| | `−` | Pin 6 (GND) | – | – |
| | `SIG` | Pin 36 | GPIO 16 | 27 |
| LED driver board | `IN1` (green) | Pin 11 | GPIO 17 | 0 |
| | `IN2` (yellow) | Pin 13 | GPIO 27 | 2 |
| | `IN3` (red) | Pin 15 | GPIO 22 | 3 |
| | `VCC` | Pin 4 (5 V) | – | – |
| | `GND` | Pin 9 (GND) | – | – |
| DC motor (fan) | `+` | Pin 4 (5 V) | – | – |
| | `−` | Pin 6 (GND) | – | – |
| | `SIG` | Pin 12 | GPIO 18 | 1 |

## LED driver board

A GPIO pin cannot safely drive an LED directly, so each LED is switched by a BC547B transistor on a small soldered perfboard:

- Each `IN` signal goes through a **10 kΩ** resistor to the transistor **base**.
- Each LED is in series with a **1 kΩ** resistor from the **+5 V** rail to the transistor **collector**.
- Each transistor **emitter** goes to **GND**.

When the Pi sets an input HIGH, the transistor conducts and the corresponding LED lights up.

BC547B facts relevant to this build: NPN, TO-92, max collector current 100 mA, max base current 5 mA, V<sub>CEO</sub> 45 V, DC gain (hFE) 110–800. Pinout (flat side facing you): **E – B – C**.

## Software setup

### 1. Enable I²C

```bash
sudo raspi-config
# Interface Options -> I2C -> Yes
```

or non-interactively:

```bash
sudo raspi-config nonint do_i2c 0
```

(In `raspi-config nonint`, `0` means *enable*. A reboot may be needed: `sudo reboot`.)

### 2. Install WiringPi

Try the package manager first:

```bash
sudo apt-get install wiringpi
```

If that fails, install the release package:

```bash
cd /tmp
wget https://github.com/WiringPi/WiringPi/releases/download/3.2/wiringpi_3.2_arm64.deb
sudo dpkg -i wiringpi_3.2_arm64.deb
```

If that fails too, build from source:

```bash
git clone https://github.com/WiringPi/WiringPi.git
cd WiringPi
./build
```

## Build and run

Save the source as `projekat.c`, then compile it with the WiringPi libraries:

```bash
gcc projekat.c -o projekat -lwiringPi -lwiringPiDev
./projekat
```

Press `Ctrl+C` to stop. A signal handler turns off all LEDs, the buzzer and the fan, clears the LCD and closes the I²C file before exiting.

### Program overview

- Initializes WiringPi, the 4-bit LCD (`lcdInit(2, 16, 4, 25, 24, 23, 22, 21, 14, 0, 0, 0, 0)`) and the I²C device `/dev/i2c-1` (slave address `0x48`).
- Selects channel AIN0 by writing control byte `0x00` to the PCF8591.
- Loops every 500 ms: reads the sensor, prints the value to the terminal and LCD, and applies the thresholds (`thresh1 = 45`, `thresh2 = 55`) to set LEDs, buzzer and fan.
- Buzzer is **active-low** (`LOW` = on, `HIGH` = off).

## Known issues and notes

- **Stale ADC reading.** The PCF8591 returns the result of the *previous* conversion. The code therefore reads twice with a `delay(10)` in between and uses the second value. Without this delay the readings can be wrong.
- **Buzzer sounds before the program starts.** Because the buzzer is active-low and GPIO pins can be in an undefined state at boot, the buzzer may beep until the program sets its pin HIGH. A pull-up (hardware or software) on the buzzer pin fixes this.
- **MQ-2 warm-up.** For datasheet-accurate readings the sensor needs a long pre-heat (the paper cites up to 24 hours of continuous operation on first use). Regular calibration and temperature/humidity compensation improve accuracy. The module's onboard trimmer sets the threshold of its digital output `D0`, which is not used by this project.
- **Thresholds are empirical.** The values 45 / 55 depend on your sensor, supply voltage and environment. Adjust `thresh1` and `thresh2` in the source to suit your setup.
- **Fan control.** The motor is switched on/off in this version. The DFRobot module supports PWM speed control, which would allow a variable fan speed.

## Ideas for future work

- Remote notifications (SMS / e-mail / push) on alarm
- Logging measurements to a local or cloud database for statistical analysis
- Web interface for live status, history charts and configuring thresholds
- Better local UI (graphical display or keypad)
- Smart-home integration over MQTT (air purifiers, extractor fans, ventilation)
- Automatic baseline tracking / self-calibration of the MQ-2 and replacement alerts
- UPS / battery backup so the sensor and buzzer keep working during a power cut
- Spoken warnings via text-to-speech

## References

1. Raspberry Pi GPIO breakout kit (T-type expansion board, breadboard, jumper cables), amazon.com
2. *Practicum in Measurement Systems Based on Microcomputers*, Prof. Dr. Jelena R. Jovanović
3. PCF8591 official datasheet
4. BC547B Transistor: Pinout, Specifications, and Datasheet, allelcoelec.com
5. Gravity: 130 DC Motor Module, dfrobot.com / kamami.pl
6. How MQ-2 Gas/Smoke Sensor Works?, lastminuteengineers.com

Full reference list with all figure sources is in the original seminar paper.
