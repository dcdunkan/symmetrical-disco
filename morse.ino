#include <LiquidCrystal_I2C.h>
#include <stdarg.h>

// I2C Address of the LCD Display module:
#define LCD_DISPLAY_I2C_ADDR 0x27
#define CHARSET_LENGTH 36
#define UNIT_DELAY 200  // time in ms that is considered as one unit.
#define MAX_INPUT_BUFFER_LENGTH 5
const char INVALID_CHARACTER = '?';


const int LCD_LINE_LENGTH = 16;

const int BUZZER_PIN = 7;
const int BUTTON_PIN = 2;
const int LED_PIN = LED_BUILTIN;  // for now
LiquidCrystal_I2C display(LCD_DISPLAY_I2C_ADDR, LCD_LINE_LENGTH, 2);

bool isHolding = false;
unsigned long holdStart;
unsigned long releaseStart;
char position = 0;

// to avoid re-execution, store and read the states.
bool partUpdated = true;
bool letterUpdated = true;
bool wordUpdated = true;

// Morse codes as defined by the International standard.
const char* MORSE_CODES[36][2] = {
  // LETTERS
  { ".-   ", "A" },
  { "-... ", "B" },
  { "-.-. ", "C" },
  { "-..  ", "D" },
  { ".    ", "E" },
  { "..-. ", "F" },
  { "--.  ", "G" },
  { ".... ", "H" },
  { "..   ", "I" },
  { ".--- ", "J" },
  { "-.-  ", "K" },
  { ".-.. ", "L" },
  { "--   ", "M" },
  { "-.   ", "N" },
  { "---  ", "O" },
  { ".--. ", "P" },
  { "--.- ", "Q" },
  { ".-.  ", "R" },
  { "...  ", "S" },
  { "-    ", "T" },
  { "..-  ", "U" },
  { "...- ", "V" },
  { ".--  ", "W" },
  { "-..- ", "X" },
  { "-.-- ", "Y" },
  { "--.. ", "Z" },
  // DIGITS
  { ".----", "1" },
  { "..---", "2" },
  { "...--", "3" },
  { "....-", "4" },
  { ".....", "5" },
  { "-....", "6" },
  { "--...", "7" },
  { "---..", "8" },
  { "----.", "9" },
  { "-----", "0" },
};

char getCharacter(char morse[]) {
  for (int i = 0; i < CHARSET_LENGTH; i++)
    if (strcmp(MORSE_CODES[i][0], morse) == 0) return MORSE_CODES[i][1][0];
  return INVALID_CHARACTER;
}

// Fill an character array with the given character.
void fillChar(char* destination, char character, int maxLength, int from = 0) {
  for (int i = from; i < maxLength; i++) *(destination + i) = character;
}

char LINE_BUFFER[LCD_LINE_LENGTH];
char INPUT_BUFFER[MAX_INPUT_BUFFER_LENGTH] = { ' ', ' ', ' ', ' ', ' ' };
char OUTPUT_BUFFER[LCD_LINE_LENGTH];

int add(int x, int y) {
  return x + y;
}
int subtract(int x, int y) {
  return x - y;
}
bool lessThan(int x, int y) {
  return x < y;
}
bool greaterThan(int x, int y) {
  return x > y;
}

void printCharByChar(char* str, int delayMs, int line, int indent, bool reverse = false) {
  int start = reverse ? strlen(str) - 1 : 0;
  auto limitFn = reverse ? greaterThan : lessThan;
  int limit = reverse ? -1 : strlen(str);
  auto updateFn = reverse ? subtract : add;
  for (int i = start; limitFn(i, limit); i = updateFn(i, 1)) {
    display.setCursor(indent + i, line);
    display.print(str[i]);
    delay(delayMs);
  }
}

uint8_t envelope1[8] = { 0x1f, 0x18, 0x14, 0x12, 0x11, 0x10, 0x10, 0x1f };
uint8_t envelope2[8] = { 0x1f, 0x03, 0x05, 0x09, 0x11, 0x01, 0x01, 0x1f };

unsigned long lastEvent = millis();

void setup() {
  // for debugging purposes:
  Serial.begin(9600);

  // Setup I/O pins
  pinMode(BUZZER_PIN, OUTPUT);
  pinMode(BUTTON_PIN, INPUT);
  pinMode(LED_PIN, OUTPUT);

  // Setup LCD I2C 16x2 display
  display.init();
  display.backlight();
  display.createChar(0, envelope1);
  display.createChar(1, envelope2);

  // Set initial states for pins and modules.
  digitalWrite(BUZZER_PIN, LOW);
  digitalWrite(LED_PIN, LOW);
  digitalWrite(BUTTON_PIN, HIGH);
  display.clear();

  // Mini splash screen
  printCharByChar("Morse Code", 50, 0, 3);
  printCharByChar("Translator", 50, 1, 3, true);
  delay(1500);
  display.clear();

  printCharByChar("Group 8B", 50, 0, 4);
  printCharByChar("58  60  61  62", 50, 1, 1, true);
  delay(1000);
  display.clear();

  lastEvent = millis();
}

void printEnvelopeChar() {
  display.write(0);
  display.write(1);
}

void loop() {
  int buttonState = digitalRead(BUTTON_PIN);

  // if (Serial.available()) {
  //   String input = Serial.readString();
  //   input.trim();
  //   if (input[0] != 0) {
  //     display.clear();
  //     display.backlight();
  //     display.home();
  //     printEnvelopeChar();
  //     display.print(" New message!");
  //     delay(3000);
  //     display.clear();

  //     char filtered[input.length()];
  //     for (int i = 0; i < input.length(); i++) {
  //       char character = input.charAt(i);
  //       filtered[i] = (i >= 65 && i <= 90) || (i >= 48 && i <= 57)
  //         ? character
  //         : i >= 97 && i <= 122
  //         ? character - 22
  //         : INVALID_CHARACTER;
  //     }

  //     display.home();
  //     printEnvelopeChar();
  //     display.setCursor(10, 0);

  //     char buffer[11];
  //     for (int i = 0; i < strlen(filtered); i++) {
  //       char c = filtered[i];

  //     }

  //     display.clear();
  //     lastEvent = millis();
  //   }
  // }

  if (buttonState == LOW && !isHolding) {
    display.backlight();
    isHolding = true, holdStart = millis(), lastEvent = millis();
    digitalWrite(BUZZER_PIN, HIGH);
  } else if (buttonState == LOW && isHolding) {
    releaseStart = millis(), lastEvent = millis();
    unsigned long holdTime = releaseStart - holdStart;
    if (holdTime > 3000) {  // hold for 3 seconds to clear the display.
      fillChar(INPUT_BUFFER, ' ', MAX_INPUT_BUFFER_LENGTH);
      fillChar(OUTPUT_BUFFER, '\0', LCD_LINE_LENGTH);
      digitalWrite(BUZZER_PIN, LOW);
      display.clear();
      partUpdated = true, letterUpdated = true, wordUpdated = true;
    } else {
      INPUT_BUFFER[position] = holdTime < 2 * UNIT_DELAY ? '.' : '-';
      partUpdated = false, letterUpdated = false, wordUpdated = false;
      printInputBuffer();
    }
  } else if (buttonState == HIGH && isHolding) {
    isHolding = false, releaseStart = millis(), lastEvent = millis();
    digitalWrite(BUZZER_PIN, LOW);
  } else if (buttonState == HIGH && !isHolding) {
    unsigned long releaseTime = millis() - releaseStart;
    if (millis() - lastEvent > 10000) {
      display.noBacklight();
    } else if (releaseTime < 3 * UNIT_DELAY && !partUpdated) {
      position = position + 1 == 5 ? 0 : position + 1;
      if (position == 0) {
        append(OUTPUT_BUFFER, getCharacter(INPUT_BUFFER));
        printInputBuffer(), printOutputBuffer();
        letterUpdated = true;
        fillChar(INPUT_BUFFER, ' ', MAX_INPUT_BUFFER_LENGTH);
      }
      partUpdated = true;
    } else if (releaseTime >= 3 * UNIT_DELAY && releaseTime < 7 * UNIT_DELAY && !letterUpdated) {
      append(OUTPUT_BUFFER, getCharacter(INPUT_BUFFER));
      printInputBuffer(), printOutputBuffer();
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

// Append character to the character array. But also shift if it touches the border.
char* append(char content[], char supplement) {
  if (strlen(content) > LCD_LINE_LENGTH) {
    for (int i = 0; i < LCD_LINE_LENGTH; i++)
      content[i] = i + 1 != LCD_LINE_LENGTH ? content[i + 1] : supplement;
  } else content[strlen(content)] = supplement;
  return content;
}

void printInputBuffer() {
  display.setCursor(0, 0);
  display.print(INPUT_BUFFER);
}

void printOutputBuffer() {
  display.setCursor(0, 1);
  display.print(OUTPUT_BUFFER);
}

// Convenience function for printing a line to the LCD
void println(int line, const char* fmt, ...) {
  va_list parameters;
  va_start(parameters, fmt);
  vsprintf(LINE_BUFFER, fmt, parameters);
  display.setCursor(0, line);
  display.print(LINE_BUFFER);
  va_end(parameters);
}