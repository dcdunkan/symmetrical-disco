/**
 * @file morse.ino
 * @brief Arduino sketch for a morse code translator machine.
 *
 * This sketch listens and measures the hold time of the input button and
 * translates the inputs to dots and dashes based on the hold time, and then
 * translates the morse input to decoded ASCII character based on the
 * International Morse Code standard. The sketch also contains a morse code
 * encoder which listens to ASCII string inputs from Serial Monitor and then
 * convert the input to morse code format. These two operations makes use of
 * an LCD display, push button and a buzzer (piezo speaker).
 *
 * @details This code is intended to demonstrate a morse code translator
 * which is similar to the Telegraph system back in the days.
 */
#include <LiquidCrystal_I2C.h>

/** The I2C address of the LCD display module in the I2C circuit */
#define LCD_DISPLAY_I2C_ADDR 0x27
/** Currently supported number of characters for the morse code translation */
#define CHARSET_LENGTH 54
/** The time delay in ms which is considered as one unit of morse input time */
#define UNIT_DELAY 200
/** The LCD module's line length */
#define LCD_LINE_LENGTH 16

/** A placeholder characters for invalid morse codes or non-printable characters */
const char INVALID_CHARACTER = '?';

/** Digital pin connected to the buzzer */
const int BUZZER_PIN = 7;
/** Digital pin connected to the button */
const int BUTTON_PIN = 2;
/** Digital pin connected to the 5mm LED */
const int LED_PIN = LED_BUILTIN;
/** The I2C LCD Display controller */
LiquidCrystal_I2C display(LCD_DISPLAY_I2C_ADDR, LCD_LINE_LENGTH, 2);

/** Maximum number of characters that can be entered to as morse inputs */
const int MAX_INPUT_BUFFER_LENGTH = 7;
/** Placeholder empty buffer to be printed for invalid characters */
const char EMPTY[MAX_INPUT_BUFFER_LENGTH + 1] = {32, 32, 32, 32, 32, 32, 32, 0};
/** Input buffer where the morse input is stored */
char INPUT_BUFFER[MAX_INPUT_BUFFER_LENGTH + 1] = {32, 32, 32, 32, 32, 32, 32, 0};
/** Output buffer where the morse output is stored */
char OUTPUT_BUFFER[LCD_LINE_LENGTH];

/** Custom character 1 (envelope-left) for fancy LCD outputs */
uint8_t ENVELOPE_LEFT[8] = {0x1f, 0x18, 0x14, 0x12, 0x11, 0x10, 0x10, 0x1f};
/** Custom character 2 (envelope-right) for fancy LCD outputs */
uint8_t ENVELOPE_RIGHT[8] = {0x1f, 0x03, 0x05, 0x09, 0x11, 0x01, 0x01, 0x1f};

/** Stores information about whether the button is being holded */
bool isHolding = false;
/** The moment in uptime when the holding started */
unsigned long holdStart;
/** The moment in uptime when the holding stopped */
unsigned long releaseStart;
/** Stores information about the time when the last event was taken place */
unsigned long lastEvent;
/** Variable to correctly manage the morse inputs */
char position = 0;

/** Stores information about morse input parts that got updated */
bool partUpdated = true;
/** Stores information about morse input letters that got updated */
bool letterUpdated = true;
/** Stores information about morse input words that got updated */
bool wordUpdated = true;

/**
 * Mapping of Morse code sequences to the corresponding decoded characters
 * as defined by the International Morse Code. Only the characters supported
 * by the Liquid Crystal Display are included in the mapping as the others
 * would not be properly rendered on to the display.
 */
const char *MORSE_CODES[CHARSET_LENGTH][2] = {
    // LETTERS
    {".-     ", "A"},
    {"-...   ", "B"},
    {"-.-.   ", "C"},
    {"-..    ", "D"},
    {".      ", "E"},
    {"..-.   ", "F"},
    {"--.    ", "G"},
    {"....   ", "H"},
    {"..     ", "I"},
    {".---   ", "J"},
    {"-.-    ", "K"},
    {".-..   ", "L"},
    {"--     ", "M"},
    {"-.     ", "N"},
    {"---    ", "O"},
    {".--.   ", "P"},
    {"--.-   ", "Q"},
    {".-.    ", "R"},
    {"...    ", "S"},
    {"-      ", "T"},
    {"..-    ", "U"},
    {"...-   ", "V"},
    {".--    ", "W"},
    {"-..-   ", "X"},
    {"-.--   ", "Y"},
    {"--..   ", "Z"},
    // NUMBERS
    {".----  ", "1"},
    {"..---  ", "2"},
    {"...--  ", "3"},
    {"....-  ", "4"},
    {".....  ", "5"},
    {"-....  ", "6"},
    {"--...  ", "7"},
    {"---..  ", "8"},
    {"----.  ", "9"},
    {"-----  ", "0"},
    // PUNCTUATIONS
    {".-.-.- ", "."},
    {"--..-- ", ","},
    {"..--.. ", "?"},
    {".----. ", "'"},
    {"-..-.  ", "/"},
    {"-.--.  ", "("},
    {"-.--.- ", ")"},
    {"---... ", ":"},
    {"-...-  ", "="},
    {".-.-.  ", "+"},
    {"-....- ", "-"},
    {".-..-. ", "\""},
    {".--.-. ", "@"},
    // SPECIAL CHARACTERS (NON-STANDARD PUNCTUATIONS)
    {"-.-.-- ", "!"},
    {".-...  ", "&"},
    {"-.-.-. ", ";"},
    {"..--.- ", "_"},
    {"...-..-", "$"},
};

/**
 * @brief Setup the connected components.
 *
 * The setup function ensures that all the components are ready
 * to be used, is in the right I/O pin mode and also resets their
 * states to the initial states.
 */
void setup()
{
  // For debugging purposes and the current implementation of
  // the decoder, Serial Monitor is required.
  Serial.begin(9600);

  // Set the pins to the right I/O modes.
  pinMode(BUZZER_PIN, OUTPUT);
  pinMode(BUTTON_PIN, INPUT_PULLUP);
  pinMode(LED_PIN, OUTPUT);

  // Prepare the Liquid Crystal Display.
  display.init();
  display.backlight();
  display.createChar(0, ENVELOPE_LEFT);
  display.createChar(1, ENVELOPE_RIGHT);

  // Reset the states of the components and variables.
  digitalWrite(BUZZER_PIN, LOW);
  digitalWrite(LED_PIN, LOW);
  digitalWrite(BUTTON_PIN, HIGH);
  display.clear();
  lastEvent = millis();

  // Show the startup animation before going into loop.
  showStartupAnimation();
}

/**
 * Get the decoded character of the given Morse code sequence.
 *
 * @param morse The Morse code sequence to be decoded.
 * @return The decoded character.
 */
char getCharacter(char morse[])
{
  for (int i = 0; i < CHARSET_LENGTH; i++)
    if (strcmp(MORSE_CODES[i][0], morse) == 0)
      return MORSE_CODES[i][1][0];
  return INVALID_CHARACTER;
}

/**
 * Get the encoded Morse code sequence of the given character.
 *
 * @param morse The character to be encoded.
 * @return The Morse code sequence of the character.
 */
char *getMorseSequence(char c)
{
  if (c >= 97 && c <= 122)
    c -= 32;
  for (int i = 0; i < CHARSET_LENGTH; i++)
    if (MORSE_CODES[i][1][0] == c)
      return MORSE_CODES[i][0];
  return EMPTY;
}

/**
 * Convenience function for filling an character array with a given character.
 *
 * @param destination Pointer to the character array to be filled.
 * @param character The character to be filled with.
 * @param maxLength The maximum length the character needs to be filled.
 * @param from The index to be filled from.
 */
void fillChar(char *destination, char character, int maxLength, int from = 0)
{
  for (int i = from; i < maxLength; i++)
    *(destination + i) = character;
}

/**
 * Convenience function for printing a string character by character onto
 * the Liquid Crystal Display to the given line from the given column.
 *
 * @param str String to be printed.
 * @param delayMs The delay between printing the characters.
 * @param line The line the string to be printed to.
 * @param indent The column the string should start printing from.
 * @param reverse If the direction need to be reverted.
 */
void printCharByChar(char *str, int delayMs, int x, int y, bool reverse = false)
{
  int length = strlen(str);
  for (int i = reverse ? length - 1 : 0; reverse ? i >= 0 : i < length; i += reverse ? -1 : 1)
  {
    display.setCursor(x + i, y);
    display.print(str[i]);
    delay(delayMs);
  }
}

/**
 * The cool startup animation showing the name and group information
 * of the project.
 */
void showStartupAnimation()
{
  // Reset the random seed and print random dots and dashes,
  // representing the Morse code.
  randomSeed(analogRead(0));
  for (int i = 0; i < LCD_LINE_LENGTH; i++)
  {
    printStringAt(i, 0, random(10) > 5 ? "." : "-");
    printStringAt(LCD_LINE_LENGTH - 1 - i, 1, random(10) > 5 ? "." : "-");
    delay(40);
  }

  printCharByChar("Morse Code", 50, 3, 0);
  printCharByChar("Translator", 50, 3, 1, true);
  delay(1500);
  display.clear();

  printStringAt(2, 0, "presented by");
  printCharByChar("Group 15", 50, 4, 1);
  delay(1000);
  printCharByChar("58  60  61  62", 50, 1, 1, true);
  delay(1000);

  display.clear();
}

/**
 * Play the buzzer for the given duration.
 * @param duration The duration of the beep.
 * @param afterDelay The time needed to wait after beeping.
 */
void playBuzzer(int duration, int afterDelay = 0)
{
  digitalWrite(BUZZER_PIN, HIGH), digitalWrite(LED_PIN, HIGH);
  delay(duration);
  digitalWrite(BUZZER_PIN, LOW), digitalWrite(LED_PIN, LOW);
  if (afterDelay != 0)
    delay(afterDelay);
}

/**
 * Checks for the state of the Serial Monitor each time and see
 * if there are any inputs from there. If so, encode the input text
 * into Morse code sequences and read it out loud.
 *
 * If there are no inputs from Serial Monitor, then go to decoding
 * mode and listen for button inputs of dots and dashes and decode
 * the input sequences to characters according to the International
 * Morse codes standards.
 */
void loop()
{
  // ENCODER
  // Listens for Serial monitor and encodes each characters to
  // Morse code sequences and buzz them out loud.
  if (Serial.available())
  {
    String input = Serial.readString();
    input.trim();      // Trim the input to sanitize it.
    if (input[0] != 0) // Check if there is actually an input.
    {
      digitalWrite(BUZZER_PIN, LOW), digitalWrite(LED_PIN, LOW);
      display.clear(), display.backlight(); // Make sure the display is on.
      printEnvelopeIcon(0, 0), display.print(" New message!");
      playBuzzer(300, 300), playBuzzer(300, 600); // Notification beep
      display.clear();
      printEnvelopeIcon(0, 0);
      printStringAt(9, 1, "^");

      // Setup the preview string.
      const int OFFSET = 6;
      char previewBuffer[13];
      char filtered[OFFSET + input.length() + OFFSET];
      fillChar(filtered, ' ', OFFSET, 0);
      for (int i = 0; i < input.length(); i++)
      {
        char character = input.charAt(i);
        filtered[i + OFFSET] = character >= 32 && character <= 125 ? character : INVALID_CHARACTER;
      }
      fillChar(filtered, ' ', OFFSET + input.length() + OFFSET, input.length() + OFFSET);

      // Print, update the preview string buffer each time a character is printed.
      for (int i = 0; i < input.length(); i++)
      {
        for (int j = 0; j < 13; j++)
          previewBuffer[j] = filtered[j + i];
        printStringAt(3, 0, previewBuffer);
        char *morse = getMorseSequence(filtered[i + OFFSET]);
        printStringAt(0, 1, morse);
        for (int j = 0; j < MAX_INPUT_BUFFER_LENGTH; j++)
        {
          if (morse[j] == ' ')
            break;
          playBuzzer((morse[j] == '.' ? 1 : 3) * UNIT_DELAY, UNIT_DELAY);
        }
        delay((morse[0] == ' ' ? 3 : 1) * UNIT_DELAY);
      }

      delay(2000); // wait a second before clearing the screen.
      display.clear();
      printStringAt(0, 1, OUTPUT_BUFFER); // restore the previous output
      lastEvent = millis();
    }
  }

  // DECODER
  // Depending on the time the button is being holded, translate the
  // input to dots and dashes and based on the time the button is not
  // holded separate the morse sequence characters, letters, and words.
  // Update the output on the Liquid Crystal Display according to the
  // updates made.
  int isButtonPressed = digitalRead(BUTTON_PIN) == LOW;

  // NOTE: The if-else-if cases are listed as different real life cases.
  // That is why they are not nested accordingly.

  // CASE 1: Started holding the button.
  if (isButtonPressed && !isHolding)
  {
    display.backlight();
    isHolding = true, holdStart = millis(), lastEvent = millis();
    digitalWrite(BUZZER_PIN, HIGH), digitalWrite(LED_PIN, HIGH);
  }
  // CASE 2: The button is still being hold.
  // According to the hold time update the input buffer.
  else if (isButtonPressed && isHolding)
  {
    releaseStart = millis();
    unsigned long holdTime = releaseStart - holdStart;
    // If the button is holded for more than 3 seconds, clear the screen
    // and clear the input/output array buffers.
    if (holdTime > 3'000)
    {
      fillChar(INPUT_BUFFER, ' ', MAX_INPUT_BUFFER_LENGTH);
      fillChar(OUTPUT_BUFFER, '\0', LCD_LINE_LENGTH);
      digitalWrite(BUZZER_PIN, LOW), digitalWrite(LED_PIN, LOW);
      display.clear();
      partUpdated = true, letterUpdated = true, wordUpdated = true;
    }
    // Otherwise, update the input buffer and print it.
    else
    {
      INPUT_BUFFER[position] = holdTime < 2 * UNIT_DELAY ? '.' : '-';
      partUpdated = false, letterUpdated = false, wordUpdated = false;
      printStringAt(0, 0, INPUT_BUFFER);
    }
    lastEvent = millis();
  }
  // CASE 3: Stopped holding the button.
  else if (!isButtonPressed && isHolding)
  {
    isHolding = false, releaseStart = millis(), lastEvent = millis();
    digitalWrite(BUZZER_PIN, LOW), digitalWrite(LED_PIN, LOW);
  }
  // CASE 4: The button is not pressed. The device is idle.
  // Decode, separate, or go to idle mode.
  else if (!isButtonPressed && !isHolding)
  {
    unsigned long releaseTime = millis() - releaseStart;
    // Disable screen backlight, if no actions have been made in the last 10s.
    if (millis() - lastEvent > 10000)
    {
      display.noBacklight();
    }
    // Dot:
    else if (releaseTime < 3 * UNIT_DELAY && !partUpdated)
    {
      position = position + 1 == MAX_INPUT_BUFFER_LENGTH ? 0 : position + 1;
      if (position == 0)
      {
        appendWithShift(OUTPUT_BUFFER, getCharacter(INPUT_BUFFER));
        printStringAt(0, 0, INPUT_BUFFER), printStringAt(0, 1, OUTPUT_BUFFER);
        letterUpdated = true;
        fillChar(INPUT_BUFFER, ' ', MAX_INPUT_BUFFER_LENGTH);
      }
      partUpdated = true;
    }
    // Dash:
    else if (releaseTime >= 3 * UNIT_DELAY && releaseTime < 7 * UNIT_DELAY && !letterUpdated)
    {
      appendWithShift(OUTPUT_BUFFER, getCharacter(INPUT_BUFFER));
      printStringAt(0, 0, INPUT_BUFFER), printStringAt(0, 1, OUTPUT_BUFFER);
      letterUpdated = true, position = 0;
      fillChar(INPUT_BUFFER, ' ', MAX_INPUT_BUFFER_LENGTH);
    }
    // Separate word:
    else if (releaseTime >= 7 * UNIT_DELAY && !wordUpdated)
    {
      // Only separate words (add spaces) only if there have been changes to input buffer.
      if (strlen(OUTPUT_BUFFER) > 0 && OUTPUT_BUFFER[strlen(OUTPUT_BUFFER) - 1] != ' ')
      {
        appendWithShift(OUTPUT_BUFFER, ' ');
      }
      wordUpdated = true, position = 0;
      fillChar(INPUT_BUFFER, ' ', MAX_INPUT_BUFFER_LENGTH);
    }
  }
}

/**
 * Append the supplement to the given character array. The defined LCD
 * line length the limit of the content. If adding the supplement hits the
 * length limit, then shift the content and then append.
 * 
 * @param content The character array to be modified.
 * @param supplement The character to be added to the content.
 * 
 * @return The modified array.
 */
char *appendWithShift(char content[], char supplement)
{
  if (strlen(content) > LCD_LINE_LENGTH)
  {
    for (int i = 0; i < LCD_LINE_LENGTH; i++)
      content[i] = i + 1 != LCD_LINE_LENGTH ? content[i + 1] : supplement;
  }
  else
    content[strlen(content)] = supplement;
  return content;
}

/**
 * Print the envelope icon to the LCD using the pre-defined custom
 * characters.
 * 
 * @param x The column
 * @param y The line
 */
void printEnvelopeIcon(short x, short y)
{
  display.setCursor(x, y);
  display.write(0);
  display.write(1);
}

/**
 * Print the string at given coordinates.
 * 
 * @param x The column
 * @param y The line
 * @param str The string to be printed
 */
void printStringAt(short x, short y, char *str)
{
  display.setCursor(x, y);
  display.print(str);
}