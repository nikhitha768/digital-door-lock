/*
 * Digital Door Lock with Keypad
 * Platform : Arduino Uno (simulated on Wokwi)
 * Features : 4-digit PIN, servo lock, LCD UI, 3-strike lockout,
 *            auto-relock, PIN change (press A), PIN stored in EEPROM
 *
 * Keys: 0-9 = digits | # = submit | * = clear | A = change PIN
 */
#include <Keypad.h>
#include <Servo.h>
#include <Wire.h>
#include <LiquidCrystal_I2C.h>
#include <EEPROM.h>

// ---------- Pins ----------
#define SERVO_PIN  3
#define LED_GREEN  10
#define LED_RED    11
#define BUZZER     12

// ---------- Settings ----------
const byte PIN_LEN = 4;
const byte MAX_TRIES = 3;
const unsigned long UNLOCK_TIME  = 5000;    // auto-relock after 5 s
const unsigned long LOCKOUT_TIME = 30000;   // 30 s lockout
const int ANGLE_LOCKED = 0, ANGLE_OPEN = 90;
const int EEPROM_ADDR = 0;
const char DEFAULT_PIN[PIN_LEN + 1] = "1234";

// ---------- Keypad ----------
const byte ROWS = 4, COLS = 4;
char keys[ROWS][COLS] = {
  {'1','2','3','A'},
  {'4','5','6','B'},
  {'7','8','9','C'},
  {'*','0','#','D'}
};
byte rowPins[ROWS] = {9, 8, 7, 6};
byte colPins[COLS] = {A0, A1, A2, A3};
Keypad keypad = Keypad(makeKeymap(keys), rowPins, colPins, ROWS, COLS);

LiquidCrystal_I2C lcd(0x27, 16, 2);
Servo lockServo;

enum State { LOCKED, UNLOCKED, LOCKOUT, CHANGE_OLD, CHANGE_NEW };
State state = LOCKED;

char storedPin[PIN_LEN + 1];
char entry[PIN_LEN + 1];
byte idx = 0;
byte tries = 0;
unsigned long stateStart = 0;
unsigned long lastLcdUpdate = 0;

// ---------- EEPROM ----------
void loadPin() {
  if (EEPROM.read(EEPROM_ADDR) != 0xA5) {          // first boot marker
    strcpy(storedPin, DEFAULT_PIN);
    savePin();
  } else {
    for (byte i = 0; i < PIN_LEN; i++) storedPin[i] = EEPROM.read(EEPROM_ADDR + 1 + i);
    storedPin[PIN_LEN] = '\0';
  }
}
void savePin() {
  EEPROM.update(EEPROM_ADDR, 0xA5);
  for (byte i = 0; i < PIN_LEN; i++) EEPROM.update(EEPROM_ADDR + 1 + i, storedPin[i]);
}

// ---------- Helpers ----------
void beep(int freq, int ms) { tone(BUZZER, freq, ms); }

void clearEntry() { idx = 0; memset(entry, 0, sizeof(entry)); }

void showPrompt(const char* line1) {
  lcd.clear();
  lcd.setCursor(0, 0); lcd.print(line1);
  lcd.setCursor(0, 1);
  for (byte i = 0; i < idx; i++) lcd.print('*');   // mask the PIN
}

void setState(State s) {
  state = s;
  stateStart = millis();
  clearEntry();
  switch (s) {
    case LOCKED:
      lockServo.write(ANGLE_LOCKED);
      digitalWrite(LED_RED, HIGH); digitalWrite(LED_GREEN, LOW);
      showPrompt("Enter PIN:");
      Serial.println(F("STATE: LOCKED"));
      break;
    case UNLOCKED:
      lockServo.write(ANGLE_OPEN);
      digitalWrite(LED_RED, LOW); digitalWrite(LED_GREEN, HIGH);
      lcd.clear(); lcd.print("ACCESS GRANTED");
      beep(1500, 200);
      Serial.println(F("STATE: UNLOCKED"));
      break;
    case LOCKOUT:
      lockServo.write(ANGLE_LOCKED);
      digitalWrite(LED_RED, HIGH); digitalWrite(LED_GREEN, LOW);
      Serial.println(F("STATE: LOCKOUT (too many wrong attempts)"));
      break;
    case CHANGE_OLD: showPrompt("Old PIN:"); Serial.println(F("STATE: CHANGE_OLD")); break;
    case CHANGE_NEW: showPrompt("New PIN:"); Serial.println(F("STATE: CHANGE_NEW")); break;
  }
}

void wrongPin() {
  tries++;
  Serial.print(F("EVENT: wrong PIN, attempt ")); Serial.println(tries);
  beep(300, 400);
  if (tries >= MAX_TRIES) {
    tries = 0;
    setState(LOCKOUT);
  } else {
    lcd.clear(); lcd.print("WRONG PIN");
    lcd.setCursor(0, 1); lcd.print("Tries left: "); lcd.print(MAX_TRIES - tries);
    delay(1200);                                    // short, acceptable pause
    setState(LOCKED);
  }
}

void handleSubmit() {
  if (idx != PIN_LEN) { beep(300, 150); return; }   // incomplete entry ignored
  bool match = (strcmp(entry, storedPin) == 0);

  switch (state) {
    case LOCKED:
      if (match) { tries = 0; setState(UNLOCKED); } else wrongPin();
      break;
    case CHANGE_OLD:
      if (match) setState(CHANGE_NEW);
      else { lcd.clear(); lcd.print("WRONG PIN"); beep(300, 400); delay(1000); setState(LOCKED); }
      break;
    case CHANGE_NEW:
      strcpy(storedPin, entry);
      savePin();
      lcd.clear(); lcd.print("PIN UPDATED");
      beep(1800, 300);
      Serial.println(F("EVENT: PIN changed"));
      delay(1200);
      setState(LOCKED);
      break;
    default: break;
  }
}

void setup() {
  Serial.begin(9600);
  pinMode(LED_GREEN, OUTPUT); pinMode(LED_RED, OUTPUT); pinMode(BUZZER, OUTPUT);
  lockServo.attach(SERVO_PIN);
  lcd.init(); lcd.backlight();
  loadPin();
  lcd.print("Digital Door Lock");
  delay(1200);
  setState(LOCKED);
}

void loop() {
  unsigned long now = millis();

  // Timed state behaviour
  if (state == UNLOCKED && now - stateStart >= UNLOCK_TIME) setState(LOCKED);

  if (state == LOCKOUT) {
    unsigned long elapsed = now - stateStart;
    if (elapsed >= LOCKOUT_TIME) { setState(LOCKED); return; }
    if (now - lastLcdUpdate >= 500) {
      lastLcdUpdate = now;
      lcd.clear(); lcd.print("LOCKED OUT!");
      lcd.setCursor(0, 1); lcd.print("Wait ");
      lcd.print((LOCKOUT_TIME - elapsed) / 1000 + 1); lcd.print("s");
      beep(2000, 100);
    }
    return;                                         // keypad ignored during lockout
  }

  char k = keypad.getKey();
  if (!k) return;

  if (state == UNLOCKED) return;                    // ignore keys while door is open

  if (k >= '0' && k <= '9') {
    if (idx < PIN_LEN) {
      entry[idx++] = k;
      beep(1000, 60);
      showPrompt(state == CHANGE_OLD ? "Old PIN:" : state == CHANGE_NEW ? "New PIN:" : "Enter PIN:");
    }
  } else if (k == '*') {
    clearEntry();
    showPrompt(state == CHANGE_OLD ? "Old PIN:" : state == CHANGE_NEW ? "New PIN:" : "Enter PIN:");
  } else if (k == '#') {
    handleSubmit();
  } else if (k == 'A' && state == LOCKED) {
    setState(CHANGE_OLD);
  }
}
