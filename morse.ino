// TODOs that I'll probably forget.
// 1. Training mode for training the dot-dash times.

#include <LiquidCrystal_I2C.h>
#include <stdarg.h>

// I2C Address of the LCD Display module:
#define LCD_DISPLAY_I2C_ADDR 0x27
#define LCD_LINE_LENGTH 16
#define CHARSET_LENGTH 36
#define UNIT_DELAY 200  // time in ms that is considered as one unit.
#define MAX_INPUT_BUFFER_LENGTH 5
const char INVALID_CHARACTER = '?';

const int BUZZER_PIN = 7;
const int BUTTON_PIN = 2;
const int LED_PIN = LED_BUILTIN;  // for now
LiquidCrystal_I2C display(LCD_DISPLAY_I2C_ADDR, LCD_LINE_LENGTH, 2);

bool wasHolding = false;
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
  return INVALID_CHARACTER;  // Negative one if not found.
}

// Fill an character array with the given character.
void fillChar(char* destination, char character, int maxLength, int from = 0) {
  for (int i = from; i < maxLength; i++) *(destination + i) = character;
}

char LINE_BUFFER[LCD_LINE_LENGTH];
char INPUT_BUFFER[MAX_INPUT_BUFFER_LENGTH] = { ' ', ' ', ' ', ' ', ' ' };
char OUTPUT_BUFFER[LCD_LINE_LENGTH];

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

  // Set initial states for pins and modules.
  digitalWrite(BUZZER_PIN, LOW);
  digitalWrite(LED_PIN, LOW);
  digitalWrite(BUTTON_PIN, HIGH);
  display.clear();
}

void loop() {
  int buttonState = digitalRead(BUTTON_PIN);
  // Serial.println(buttonState);

  if (buttonState == LOW && !wasHolding) {        // started holding
    wasHolding = true, holdStart = millis();      // just record the values.
  } else if (buttonState == LOW && wasHolding) {  // still holding
    // TODO: Move the state management from using the release time to hold time.
  } else if (buttonState == HIGH && wasHolding) {  // stopped holding
    // Depending on the hold time, differentiate dots and dashes and push them to the input buffer.
    wasHolding = false, releaseStart = millis();
    unsigned long holdTime = releaseStart - holdStart;

    INPUT_BUFFER[position] = holdTime < 2 * UNIT_DELAY ? '.' : '-';
    partUpdated = false, letterUpdated = false, wordUpdated = false;  // condaminate them all.

    println(0, INPUT_BUFFER);                       // print the updated input buffer.
  } else if (buttonState == HIGH && !wasHolding) {  // doing nothing
    unsigned long releaseTime = millis() - releaseStart;

    if (releaseTime < 3 * UNIT_DELAY && !partUpdated) {
      // TIME DELAY 1: MORSE CODE PART (DOT/DASH)
      // Just update the position.
      position = position + 1 == 5 ? 0 : position + 1;
      partUpdated = true;
    } else if (releaseTime >= 3 * UNIT_DELAY && releaseTime < 7 * UNIT_DELAY && !letterUpdated) {
      append(OUTPUT_BUFFER, getCharacter(INPUT_BUFFER));  // append the decoded character to the output

      println(0, INPUT_BUFFER);
      println(1, OUTPUT_BUFFER);

      letterUpdated = true, position = 0;                    // the letter is now updated.
      fillChar(INPUT_BUFFER, ' ', MAX_INPUT_BUFFER_LENGTH);  // clear and prepare input buffer for the next letter.
    } else if (releaseTime >= 7 * UNIT_DELAY && !wordUpdated) {
      // TIME DELAY 3: SEPARATING WORDS
      // NOTE: The check can be true for the very first morse letter input, because the releaseTime would be a
      // higher value since the releaseStart is set to ZERO and the word wouldn't have been updated yet.

      // To prevent adding spaces unnecessarily, only append the <SPACE> if
      // - there have been some content added before it. (handles the VERY first input event)
      // - the previous character isn't an space. we don't want multiple spaces ofc.
      if (strlen(OUTPUT_BUFFER) > 0 && OUTPUT_BUFFER[strlen(OUTPUT_BUFFER) - 1] != ' ') {
        append(OUTPUT_BUFFER, ' ');
      }
      wordUpdated = true, position = 0;                      // the word is updated now.
      fillChar(INPUT_BUFFER, ' ', MAX_INPUT_BUFFER_LENGTH);  // clear and prepare input buffer for the next letter.
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

// Convenience function for printing a line to the LCD
void println(int line, const char* fmt, ...) {
  va_list parameters;
  va_start(parameters, fmt);
  vsprintf(LINE_BUFFER, fmt, parameters);
  display.setCursor(0, line);
  display.print(LINE_BUFFER);
  va_end(parameters);
}