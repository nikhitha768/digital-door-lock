# Digital Door Lock with Keypad

An embedded-systems project simulated on [Wokwi](https://wokwi.com) (no physical hardware needed).
An Arduino Uno reads a 4-digit PIN from a 4x4 keypad, drives a servo as the door lock, and shows
status on an I2C LCD. Includes a 3-strike lockout, auto-relock and PIN change, with the PIN
stored in EEPROM.

## Features
- 4-digit PIN entry (masked with `*` on the LCD)
- Servo lock: 0 deg = locked, 90 deg = open
- Auto-relock after 5 seconds
- 3 wrong attempts triggers a 30-second lockout with countdown and alarm
- Change PIN (press `A`, enter old PIN, enter new PIN)
- PIN saved in EEPROM, so it survives resets
- Serial log of all events for debugging

## Components (simulated)
| Part | Role | Pin |
|------|------|-----|
| Arduino Uno | Microcontroller | - |
| 4x4 Membrane keypad | PIN input | Rows D9-D6, Cols A0-A3 |
| LCD1602 (I2C) | User interface | A4 (SDA), A5 (SCL) |
| Servo | Lock mechanism | D3 |
| Green LED | Unlocked indicator | D10 |
| Red LED | Locked indicator | D11 |
| Buzzer | Feedback / alarm | D12 |

## Keys
| Key | Action |
|-----|--------|
| 0-9 | Enter digit |
| # | Submit |
| * | Clear entry |
| A | Change PIN (only when locked) |

Default PIN: **1234**

## State machine
| State | Entered when | Next state |
|-------|-------------|-----------|
| LOCKED | Startup, relock, after wrong PIN | UNLOCKED (correct PIN), LOCKOUT (3 fails), CHANGE_OLD (key A) |
| UNLOCKED | Correct PIN | LOCKED (after 5 s) |
| LOCKOUT | 3 wrong attempts | LOCKED (after 30 s) |
| CHANGE_OLD | Key A pressed | CHANGE_NEW (old PIN ok), LOCKED (wrong) |
| CHANGE_NEW | Old PIN verified | LOCKED (new PIN saved) |

## How to run
1. Go to wokwi.com > New Project > **Arduino Uno**.
2. Paste `src/main.ino` into the sketch, `diagram.json` into the diagram tab, and add the
   libraries from `libraries.txt`.
3. Start the simulation and click the keypad buttons.

## Test cases
| # | Action | Expected result |
|---|--------|-----------------|
| 1 | Enter 1234 then # | ACCESS GRANTED, servo opens, green LED, relocks in 5 s |
| 2 | Enter 1111 then # | WRONG PIN, "Tries left: 2" |
| 3 | Enter wrong PIN 3 times | LOCKED OUT, 30 s countdown, alarm |
| 4 | Enter 12 then * | Entry cleared |
| 5 | Enter 12 then # | Ignored (incomplete) |
| 6 | A, 1234, #, 5678, # | PIN UPDATED |
| 7 | Reset, then enter 5678 | Still works (EEPROM persistence) |

## Concepts used
Finite state machine, keypad matrix scanning, PWM servo control, I2C, EEPROM persistence,
non-blocking timing with `millis()`, input validation, brute-force protection.

## License
MIT
