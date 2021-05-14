

// Arduino Spede
// a Reaction Time Tester
//
// Copyright (c) 2013 Petri Häkkinen
//
// Permission is hereby granted, free of charge, to any person
// obtaining a copy of this software and associated documentation
// files (the "Software"), to deal in the Software without
// restriction, including without limitation the rights to use,
// copy, modify, merge, publish, distribute, sublicense, and/or sell
// copies of the Software, and to permit persons to whom the
// Software is furnished to do so, subject to the following
// conditions:
//
// The above copyright notice and this permission notice shall be
// included in all copies or substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
// EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES
// OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
// NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT
// HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY,
// WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
// FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR
// OTHER DEALINGS IN THE SOFTWARE.

#include <EEPROM.h>
//#include <LiquidCrystal.h>
#include <Wire.h> 
#include <LiquidCrystal_I2C.h>

// initialize the library by associating any needed LCD interface pin
// with the arduino pin number it is connected to
//const int rs = 12, en = 11, d4 = 5, d5 = 4, d6 = 3, d7 = 2;
const int rs = 14, en = 15, d4 = 16, d5 = 17, d6 = 18, d7 = 19;
//LiquidCrystal lcd(rs, en, d4, d5, d6, d7);
// Set the LCD address to 0x27 for a 16 chars and 2 line display
LiquidCrystal_I2C lcd(0x27, 16, 2);


// Arduino pins connected to latch, clock and data pins of the 74HC595 chip
//const int latchPin = 7;
//const int clockPin = 8;
//const int dataPin = 9;
const int latchPin = 20;
const int clockPin = 21;
const int dataPin = 22;

// Arduino pin connected to the speaker
const int tonePin = 9;

// Arduino pins connected to transistors controlling the digits
const int enableDigits[] = { 13,10,11,12 };

// Arduino pins connected to leds
const int leds[] = { 2,3,4,5 };

// Arduino pins connected to buttons
const int buttons[] = { 6,7,8,10 };

// Frequencies of tones played when buttons are pressed
const int toneFreq[] = { 277, 311, 370, 415 };  // CS4, DS4, FS4, GS4

// Segment bits for numbers 0-9
int digits[10] = {
  B1111101, // 0  ABCDEF-
  B1000001, // 1  -BC----
  B1011110, // 2  AB-DE-G
  B1010111, // 3  ABCD--G  
  B1100011, // 4  -BC--FG
  B0110111, // 5  A-CD-EF
  B0111111, // 6  A-BCDEF
  B1010001, // 7  ABC----
  B1111111, // 8  ABCDEFG
  B1110111, // 9  ABCD-FG
};

enum {
  STATE_START_MENU,
  STATE_GAME_OVER,
  STATE_GAME_STANDARD,
  STATE_GAME_SPEED,
  STATE_GAME_MEMORY,
  STATE_GAME_1V1
};

int score = 0;
int led = 0;
int prevLed = 0;
int nextTimer = 0;
long globalGameTimer = 0;
int level = 0;
int hiscore = 0;
int startMenuTimer = 0;
int prevButtonState[] = { HIGH, HIGH, HIGH, HIGH };
int state = STATE_START_MENU;
int prevState = STATE_GAME_OVER;

// Read hiscore value from EEPROM
void readHiscore() {
  hiscore = (EEPROM.read(0) << 8) + EEPROM.read(1);
  
  // EEPROM initial value is FFFF
  if(hiscore == 0xffff)
    hiscore = 0;
}

// Write hiscore value to EEPROM
void writeHiscore() {
  EEPROM.write(0, hiscore >> 8);
  EEPROM.write(1, hiscore & 0xff);
}

void setup() {
  // set up the LCD's number of columns and rows:
  //lcd.backlight();
  //lcd.begin(16, 2);

  Serial.begin(9600);
  
  lcd.begin();
  
  pinMode(latchPin, OUTPUT);
  pinMode(clockPin, OUTPUT);
  pinMode(dataPin, OUTPUT);
  
  for(int i = 0; i < 4; i++)
    pinMode(enableDigits[i], OUTPUT);
  
  for(int i = 0; i < 4; i++)
    pinMode(leds[i], OUTPUT);

  // enable pull up resistors for button pins
  for(int i = 0; i < 4; i++)
    pinMode(buttons[i], INPUT_PULLUP);
    
  readHiscore();
}

// Updates display with current score.
// Flashes 4 digits quickly on the display.
// Display is turned off if enable is false.
void updateDisplay(int score, boolean enable, boolean forceDisplayUpdate) {
  int s = score;
  
  //update LCD
  if(state != prevState ||
      forceDisplayUpdate){
    if(state == STATE_START_MENU){
      lcd.clear();
      lcd.print("   Select Game");
      lcd.setCursor(0, 1);
      lcd.print("STD SPD  MEM 1v1");
    }
    else if(state == STATE_GAME_OVER){
      lcd.clear();
      lcd.print("GAME_OVER");
    }
    else if(state == STATE_GAME_STANDARD){
      lcd.clear();
      lcd.print("GAME_STANDARD");
      lcd.setCursor(0, 1);
      lcd.print(score);
    }
    else if(state == STATE_GAME_SPEED){
      lcd.clear();
      lcd.print("GAME_SPEED");
      lcd.setCursor(0, 1);
      lcd.print(score);
      lcd.print("      ");
      lcd.print((millis() - globalGameTimer)/1000);
    }
    else if(state == STATE_GAME_MEMORY){
      lcd.clear();
      lcd.print("GAME_MEMORY");
    }
    else if(state == STATE_GAME_1V1){
      lcd.clear();
      lcd.print("GAME_1V1");
      lcd.setCursor(0, 1);
      lcd.print(score);
    }
    prevState = state;
  }
  
  for(int pos = 0; pos < 4; pos++) {
    int digit = s % 10;
    s /= 10;
      
    // turn off all digits
    for( int i = 0; i < 4; i++ )
      digitalWrite(enableDigits[i], LOW);

    delayMicroseconds(500);
    
    // set latch low so digits won't change while sending bits 
    digitalWrite(latchPin, LOW);

    // shift out the bits
    shiftOut(dataPin, clockPin, MSBFIRST, digits[digit]);  
  
    // set latch high so the LEDs will light up
    digitalWrite(latchPin, HIGH);
  
    delayMicroseconds(500);
      
    // turn on one digit
    if(enable)
      digitalWrite(enableDigits[pos], HIGH);

    delayMicroseconds(2000);
  }
  
}

// Updates the start menu. Switch between previous score and hiscore on the display.
// Start a new game when a button is pressed. Clear the hiscore is all buttons are held down for 2 seconds.
void startMenu() {
  int s = score;

  // flick between previous score and hiscore display
  startMenuTimer = (startMenuTimer + 1) % 2000;
  if(startMenuTimer >= 1000)
    s = hiscore;
    
  updateDisplay(s, startMenuTimer < 975 || (startMenuTimer > 1000 && startMenuTimer < 1975), false);
  
  // read button state
  int buttonState = 0;
  for(int i = 0; i < 4; i++)
    if(digitalRead(buttons[i]) == LOW)
      buttonState |= 1<<i;
  
  // reset hiscore if all buttons are held down for 2 seconds
  static long resetHiscoreTimer = 0;
  if(buttonState == 15) {
    if(resetHiscoreTimer == 0)
      resetHiscoreTimer = millis();
    if(millis() - resetHiscoreTimer > 2000) {
      updateDisplay(0, false, false);
      tone(tonePin, 500, 500);
      hiscore = 0;
      writeHiscore();
      delay(700);
      resetHiscoreTimer = 0;
    }
  } else {
    resetHiscoreTimer = 0;
  }
  
  // start new game if a single button is pressed for 100ms
  static int startNewGameTimer = 0;
  if(buttonState == 1 || buttonState == 2 || buttonState == 4 || buttonState == 8) {
    if(startNewGameTimer == 0)
      startNewGameTimer = millis();
    if(millis() - startNewGameTimer > 50) {  
      // start new game
      if(buttonState == 1)
      {
        state = STATE_GAME_STANDARD;
      }
      else if(buttonState == 2)
      {
        state = STATE_GAME_SPEED;
      }
      else if(buttonState == 4)
      {
        state = STATE_GAME_MEMORY;
      }
      else if(buttonState == 8)
      {
        state = STATE_GAME_1V1;
      }

      startNewGame();
      updateDisplay(score, false, false);
      
      delay(2000);
      
      startNewGameTimer = 0;
    }
  } else {
    startNewGameTimer = 0;
  }
}

int gameSpeed;

// Prepares game state for a new game.
void startNewGame() {
  //state = STATE_GAME;
  score = 0;
  level = -1;
  led = -1;
  prevLed = -1;
  nextTimer = 0;
  globalGameTimer = 0;
  gameSpeed = 100;  // initial game speed, higher values are slower 450 default
 
  for(int i = 0; i < 4; i++) 
    prevButtonState[i] = HIGH;

  // set random seed, so that every game has a different sequence
  randomSeed(millis());
}

//=====================================================
// Game 1: Standard Game
//=====================================================

void playStandardGame() {
  // update time
  nextTimer--;
  
  if(nextTimer < 0) {
    // game over if player is too slow
    if(led >= 0) {
      gameOver();
      return;
    }

    led = random(4);
    
    // make consequent same leds less probable
    if(led == prevLed && random(10) < 6)
      led = random(4);
    prevLed = led;
    
    //original gameSpeed algorith.
    nextTimer = max(150 * pow(1.6, -level*0.05), 10);

    //spede2 algorith
    /*
    if(level < 40)
      gameSpeed -= 3;
    else if(level < 80)
      gameSpeed -= 2;
    else if(level < 120)
      gameSpeed--;
    else
      gameSpeed -= level & 1;
    gameSpeed = max(gameSpeed, 100);
    nextTimer = gameSpeed;
    */
    
    level++;
    
    tone(tonePin, toneFreq[led], nextTimer * 8);
    
    score = level;
  }
  
  // update leds
  for(int i = 0; i < 4; i++)
    digitalWrite(leds[i], led == i || (digitalRead(buttons[i]) == LOW && nextTimer > 5) ? HIGH : LOW);
   
  updateDisplay(score, true, false);
  
  // read input   
  for(int i = 0; i < 4; i++) {
    int but = digitalRead(buttons[i]);
    if(but == LOW && prevButtonState[i] == HIGH) {
      // ignore button press if time since last press is too short
      if( led >= 0 ) { //&& millis() - lastButtonPress > 50 ) { 
        // correct button pressed?
        if( i == led ) {
          score++;
          updateDisplay(score, true, true);
          led = -1;  // turn off led
        } else {
          gameOver();
        }
        //lastButtonPress = millis();
        noTone(tonePin);
      }
    }
    prevButtonState[i] = but;
  }
}

//=====================================================
// Game 2: Speed Game
//=====================================================
void playSpeedGame() {
  int speedGameLengthMillis = 10000;
  //update screen
  updateDisplay(score, true, false);

  if(globalGameTimer == 0){
    globalGameTimer = millis();
  }
  else{
    //Serial.println("Global: " + globalGameTimer);
    
    if(nextTimer <= 0) {
      led = random(4);
      // make consequent same leds less probable
      if(led == prevLed)
        led = random(4);
      if(led == prevLed)
        led = random(4);
      prevLed = led;
      nextTimer = 1;
    }

    // update leds
    for(int i = 0; i < 4; i++)
    digitalWrite(leds[i], led == i || (digitalRead(buttons[i]) == LOW && nextTimer > 5) ? HIGH : LOW);
    //digitalWrite(leds[i], led == i);

    //tone(tonePin, toneFreq[led], nextTimer * 8);
    tone(tonePin, toneFreq[led], 100);

    // read input   
    for(int i = 0; i < 4; i++) {
      int but = digitalRead(buttons[i]);
      if(but == LOW && prevButtonState[i] == HIGH) {
        // ignore button press if time since last press is too short
        if( led >= 0 ) { //&& millis() - lastButtonPress > 50 ) { 
          // correct button pressed?
          if( i == led ) {
            score++;
            updateDisplay(score, true, true);
            led = -1;  // turn off led
            nextTimer = 0;
          } else {
            gameOver();
          }
          //lastButtonPress = millis();
          noTone(tonePin);
        }
      }
      prevButtonState[i] = but;
    }
  }

  //Check if time has run out
  if((millis() - globalGameTimer) > speedGameLengthMillis){
    gameOver();
  }

  //update display with Game name
  //start speed timer eg 30s
  //activate random led
  //read input == to led
  //score++
  //when timer end, game over, display score
  //gameover()
}

//=====================================================
// Game 3: Memory Game
//=====================================================
void playMemoryGame() {
  updateDisplay(score, true, false);
  delay(2000);
  gameOver();
}

//=====================================================
// Game 4: 1v1 Game
//=====================================================
void play1v1Game() {
  //update screen
  updateDisplay(score, true, false);
  
  //random delay as game starts
  if(globalGameTimer == 0){
    delay(5000 - random(4000));
    globalGameTimer = millis();

    //light up LEDS
    digitalWrite(leds[0], HIGH);
    digitalWrite(leds[3], HIGH);
  }
  else{
    //start timer
  
    // read input   
    for(int i = 0; i < 4; i++) {
      int but = digitalRead(buttons[i]);
      if(but == LOW && prevButtonState[i] == HIGH) {
        // ignore button press if time since last press is too short

        score = millis() - globalGameTimer;    //delay(gameStartDelayTimer);
        
        if(i == 0){
          updateDisplay(score, true, true);
          digitalWrite(leds[0], HIGH);
          digitalWrite(leds[3], LOW);
        }
        else if(i == 3){
          updateDisplay(score, true, true);
          digitalWrite(leds[0], LOW);
          digitalWrite(leds[3], HIGH);
        }
        else{
          return;
        }
        
        delay(2000);
        gameOver();
      }
      prevButtonState[i] = but;
    }
  }

  //display results
}

// Game over. Play a game over sound and blink score.
void gameOver() {
  tone(tonePin, 250, 2500);

  // new hiscore?
  if(score > hiscore) {
    hiscore = score;
    writeHiscore();
  }
  
  // turn on leds
  for(int i = 0; i < 4; i++)
    digitalWrite(leds[i], HIGH);
    
  // flash display
  for(int i = 0; i < 70*5; i++) {
    if(i == 70*2)
      tone(tonePin, 200, 2000);    
    boolean enable = 1 - (i/60) & 1;
    updateDisplay(score, enable, false);
  }
  
  // turn off leds
  for(int i = 0; i < 4; i++)
    digitalWrite(leds[i], LOW);
   updateDisplay(score, false, false);
  
  // enter menu
  //delay(1000);
  state = STATE_START_MENU;
  startMenuTimer = 0;
}

// Main loop. Update menu, game or game over depending on current state.
void loop() {
  if(state == STATE_START_MENU)
    startMenu();
  else if(state == STATE_GAME_STANDARD)
    playStandardGame();
  else if(state == STATE_GAME_SPEED)
    playSpeedGame();
  else if(state == STATE_GAME_MEMORY)
    playMemoryGame();
  else if(state == STATE_GAME_1V1)
    play1v1Game();
  else
    gameOver();
}
