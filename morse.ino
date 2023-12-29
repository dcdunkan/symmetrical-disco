#include <LiquidCrystal_I2C.h>

#define LCD_DISPLAY_I2C_ADDR 0x27
#define CHARSET_LENGTH 54
#define UNIT_DELAY 200
#define LCD_LINE_LENGTH 16

const char INVALID_CHARACTER = '?';

const int BUZZER_PIN = 7;
const int BUTTON_PIN = 2;
const int LED_PIN = LED_BUILTIN;
LiquidCrystal_I2C display(LCD_DISPLAY_I2C_ADDR, LCD_LINE_LENGTH, 2);

const int MAX_INPUT_BUFFER_LENGTH = 7;
const char EMPTY[MAX_INPUT_BUFFER_LENGTH + 1] = { 32, 32, 32, 32, 32, 32, 32, 0 };
char INPUT_BUFFER[MAX_INPUT_BUFFER_LENGTH + 1] = { 32, 32, 32, 32, 32, 32, 32, 0 };
char OUTPUT_BUFFER[LCD_LINE_LENGTH];

uint8_t ENVELOPE_LEFT[8] = { 0x1f, 0x18, 0x14, 0x12, 0x11, 0x10, 0x10, 0x1f };
uint8_t ENVELOPE_RIGHT[8] = { 0x1f, 0x03, 0x05, 0x09, 0x11, 0x01, 0x01, 0x1f };

bool isHolding = false;
unsigned long holdStart;
unsigned long releaseStart;
unsigned long lastEvent;
char position = 0;

bool partUpdated = true;
bool letterUpdated = true;
bool wordUpdated = true;

const char* MORSE_CODES[CHARSET_LENGTH][2] = {
  // LETTERS
  { ".-     ", "A" },
  { "-...   ", "B" },
  { "-.-.   ", "C" },
  { "-..    ", "D" },
  { ".      ", "E" },
  { "..-.   ", "F" },
  { "--.    ", "G" },
  { "....   ", "H" },
  { "..     ", "I" },
  { ".---   ", "J" },
  { "-.-    ", "K" },
  { ".-..   ", "L" },
  { "--     ", "M" },
  { "-.     ", "N" },
  { "---    ", "O" },
  { ".--.   ", "P" },
  { "--.-   ", "Q" },
  { ".-.    ", "R" },
  { "...    ", "S" },
  { "-      ", "T" },
  { "..-    ", "U" },
  { "...-   ", "V" },
  { ".--    ", "W" },
  { "-..-   ", "X" },
  { "-.--   ", "Y" },
  { "--..   ", "Z" },
  // NUMBERS
  { ".----  ", "1" },
  { "..---  ", "2" },
  { "...--  ", "3" },
  { "....-  ", "4" },
  { ".....  ", "5" },
  { "-....  ", "6" },
  { "--...  ", "7" },
  { "---..  ", "8" },
  { "----.  ", "9" },
  { "-----  ", "0" },
  // PUNCTUATIONS
  { ".-.-.- ", "." },
  { "--..-- ", "," },
  { "..--.. ", "?" },
  { ".----. ", "'" },
  { "-..-.  ", "/" },
  { "-.--.  ", "(" },
  { "-.--.- ", ")" },
  { "---... ", ":" },
  { "-...-  ", "=" },
  { ".-.-.  ", "+" },
  { "-....- ", "-" },
  { ".-..-. ", "\"" },
  { ".--.-. ", "@" },
  // SPECIAL CHARACTERS (NON-STANDARD PUNCTUATIONS)
  { "-.-.-- ", "!" },
  { ".-...  ", "&" },
  { "-.-.-. ", ";" },
  { "..--.- ", "_" },
  { "...-..-", "$" },
};

void setup() {
  Serial.begin(9600);

  pinMode(BUZZER_PIN, OUTPUT);
  pinMode(BUTTON_PIN, INPUT);
  pinMode(LED_PIN, OUTPUT);

  display.init();
  display.backlight();
  display.createChar(0, ENVELOPE_LEFT);
  display.createChar(1, ENVELOPE_RIGHT);

  digitalWrite(BUZZER_PIN, LOW);
  digitalWrite(LED_PIN, LOW);
  digitalWrite(BUTTON_PIN, HIGH);
  display.clear();

  showStartupAnimation();  // GAYEST show off ever
  lastEvent = millis();
}

char getCharacter(char morse[]) {
  for (int i = 0; i < CHARSET_LENGTH; i++)
    if (strcmp(MORSE_CODES[i][0], morse) == 0) return MORSE_CODES[i][1][0];
  return INVALID_CHARACTER;
}

char* getMorseSequence(char c) {
  if (c >= 97 && c <= 122) c -= 32;
  for (int i = 0; i < CHARSET_LENGTH; i++)
    if (MORSE_CODES[i][1][0] == c) return MORSE_CODES[i][0];
  return EMPTY;
}

void fillChar(char* destination, char character, int maxLength, int from = 0) {
  for (int i = from; i < maxLength; i++) *(destination + i) = character;
}

void printCharByChar(char* str, int delayMs, int line, int indent, bool reverse = false) {
  int length = strlen(str);
  for (int i = reverse ? length - 1 : 0; reverse ? i >= 0 : i < length; i += reverse ? -1 : 1) {
    display.setCursor(indent + i, line);
    display.print(str[i]);
    delay(delayMs);
  }
}

void showStartupAnimation() {
  randomSeed(analogRead(0));
  for (int i = 0; i < LCD_LINE_LENGTH; i++) {
    printStringAt(i, 0, random(10) > 5 ? "." : "-");
    printStringAt(LCD_LINE_LENGTH - 1 - i, 1, random(10) > 5 ? "." : "-");
    delay(40);
  }

  printCharByChar("Morse Code", 50, 0, 3);
  printCharByChar("Translator", 50, 1, 3, true);
  delay(1500);
  display.clear();

  printStringAt(2, 0, "presented by");
  printCharByChar("Group 15", 50, 1, 4);
  delay(1000);
  printCharByChar("58  60  61  62", 50, 1, 1, true);
  delay(1000);

  display.clear();
}

void playBuzzer(int duration, int afterDelay = 0) {
  digitalWrite(BUZZER_PIN, HIGH);
  delay(duration);
  digitalWrite(BUZZER_PIN, LOW);
  if (afterDelay != 0) delay(afterDelay);
}

void loop() {
  // ENCODER
  if (Serial.available()) {
    String input = Serial.readString();
    input.trim();
    if (input[0] != 0) {
      digitalWrite(BUZZER_PIN, LOW);
      display.clear(), display.backlight();
      printEnvelopeIcon(0, 0), display.print(" New message!");
      playBuzzer(300, 300), playBuzzer(300, 600);
      display.clear();
      printEnvelopeIcon(0, 0);
      printStringAt(9, 1, "^");

      const int OFFSET = 6;
      char previewBuffer[13];
      char filtered[OFFSET + input.length() + OFFSET];

      fillChar(filtered, ' ', OFFSET, 0);
      for (int i = 0; i < input.length(); i++) {
        char character = input.charAt(i);
        filtered[i + OFFSET] = character >= 32 && character <= 125 ? character : INVALID_CHARACTER;
      }
      fillChar(filtered, ' ', OFFSET + input.length() + OFFSET, input.length() + OFFSET);

      for (int i = 0; i < input.length(); i++) {
        for (int j = 0; j < 13; j++) previewBuffer[j] = filtered[j + i];
        printStringAt(3, 0, previewBuffer);
        char* morse = getMorseSequence(filtered[i + OFFSET]);
        printStringAt(0, 1, morse);
        for (int j = 0; j < MAX_INPUT_BUFFER_LENGTH; j++) {
          if (morse[j] == ' ') break;
          playBuzzer((morse[j] == '.' ? 1 : 3) * UNIT_DELAY, UNIT_DELAY);
        }
        delay(3 * UNIT_DELAY);
      }

      delay(2000);
      display.clear();
      printStringAt(0, 1, OUTPUT_BUFFER);
      lastEvent = millis();
    }
  }

  // DECODER
  int buttonState = digitalRead(BUTTON_PIN);
  if (buttonState == LOW && !isHolding) {
    display.backlight();
    isHolding = true, holdStart = millis(), lastEvent = millis();
    digitalWrite(BUZZER_PIN, HIGH);
  } else if (buttonState == LOW && isHolding) {
    releaseStart = millis(), lastEvent = millis();
    unsigned long holdTime = releaseStart - holdStart;
    if (holdTime > 3000) {
      fillChar(INPUT_BUFFER, ' ', MAX_INPUT_BUFFER_LENGTH);
      fillChar(OUTPUT_BUFFER, '\0', LCD_LINE_LENGTH);
      digitalWrite(BUZZER_PIN, LOW);
      display.clear();
      partUpdated = true, letterUpdated = true, wordUpdated = true;
    } else {
      INPUT_BUFFER[position] = holdTime < 2 * UNIT_DELAY ? '.' : '-';
      partUpdated = false, letterUpdated = false, wordUpdated = false;
      printStringAt(0, 0, INPUT_BUFFER);
    }
  } else if (buttonState == HIGH && isHolding) {
    isHolding = false, releaseStart = millis(), lastEvent = millis();
    digitalWrite(BUZZER_PIN, LOW);
  } else if (buttonState == HIGH && !isHolding) {
    unsigned long releaseTime = millis() - releaseStart;
    if (millis() - lastEvent > 10000) {
      display.noBacklight();
    } else if (releaseTime < 3 * UNIT_DELAY && !partUpdated) {
      position = position + 1 == MAX_INPUT_BUFFER_LENGTH ? 0 : position + 1;
      if (position == 0) {
        append(OUTPUT_BUFFER, getCharacter(INPUT_BUFFER));
        printStringAt(0, 0, INPUT_BUFFER), printStringAt(0, 1, OUTPUT_BUFFER);
        letterUpdated = true;
        fillChar(INPUT_BUFFER, ' ', MAX_INPUT_BUFFER_LENGTH);
      }
      partUpdated = true;
    } else if (releaseTime >= 3 * UNIT_DELAY && releaseTime < 7 * UNIT_DELAY && !letterUpdated) {
      append(OUTPUT_BUFFER, getCharacter(INPUT_BUFFER));
      printStringAt(0, 0, INPUT_BUFFER), printStringAt(0, 1, OUTPUT_BUFFER);
      letterUpdated = true, position = 0;
      fillChar(INPUT_BUFFER, ' ', MAX_INPUT_BUFFER_LENGTH);
    } else if (releaseTime >= 7 * UNIT_DELAY && !wordUpdated) {
      if (strlen(OUTPUT_BUFFER) > 0 && OUTPUT_BUFFER[strlen(OUTPUT_BUFFER) - 1] != ' ') {
        append(OUTPUT_BUFFER, ' ');
      }
      wordUpdated = true, position = 0;
      fillChar(INPUT_BUFFER, ' ', MAX_INPUT_BUFFER_LENGTH);
    }
  }
}

char* append(char content[], char supplement) {
  if (strlen(content) > LCD_LINE_LENGTH) {
    for (int i = 0; i < LCD_LINE_LENGTH; i++)
      content[i] = i + 1 != LCD_LINE_LENGTH ? content[i + 1] : supplement;
  } else content[strlen(content)] = supplement;
  return content;
}

void printEnvelopeIcon(short x, short y) {
  display.setCursor(x, y);
  display.write(0);
  display.write(1);
}

void printStringAt(short x, short y, char* str) {
  display.setCursor(x, y);
  display.print(str);
}