/**********************************************************************************  
  KiKlok PROJECT.  An accurate MIDI clock generator
  Franck Touanen - Dec 2017.
************************************************************************************/

#include <string.h>
#include "build_number_defines.h"
#include <MIDI.h>
#include <EEPROM.h>
#include <LiquidCrystal.h>
#include <PulseOut.h>

// =================================================================================
// GLOBALS DEFINES AND SETTINGS 
// ---------------------------------------------------------------------------------

#define               LED                 13
#define               CVGATE_PIN          17 // A3

#define btnRIGHT    1
#define btnUP       2
#define btnDOWN     3
#define btnLEFT     4
#define btnSELECT   5
#define btnRELEASED 0

// Remap buttons to functions
#define btnPLAY     btnLEFT
#define btnSTOP     btnSELECT
#define btnSTOP2    btnENC+1000
#define btnMODE     btnRIGHT
#define btnBPM_INC  btnUP
#define btnBPM_DEC  btnDOWN


boolean       isPlaying         = false;  // Playing mode (Realtime play)
boolean       isPaused          = false;  // Pause mode (Realtime midi)
boolean       sendStart         = false;
boolean       sendResync        = false;
unsigned long songPointerPos    = 0;      // Song Pointer Position

float         bpm               = 120.0;  // the bpm for the clock (30.0-300.0)
float         clockTick         ;         // To be converted in microseconds
unsigned long lastDebounceTime  = 0;      // the last time the output pin was toggled
unsigned long debounceDelay     = 300;    // the keypad pressed debounce time;

unsigned long newMicros;  // Microsecs counters
unsigned long oldMicros;

PulseOut CVPulse(CVGATE_PIN,15);

// Special characters to display on the LCD
byte char_Play[8] = {
  B11000,
  B11100,
  B11110,
  B11111,
  B11110,
  B11100,
  B11000,
};
#define CHARPLAY  0

byte char_Pause[8] = {
  B11011,
  B11011,
  B11011,
  B11011,
  B11011,
  B11011,
  B11011,
};
#define CHARPAUSE  1

byte char_Stop[8] = {
  B00000,
  B11111,
  B11111,
  B11111,
  B11111,
  B11111,
  B00000,
};
#define CHARSTOP  2

byte char_Bar1[8] = {
  B11111,
  B10101,
  B10101,
  B10101,
  B10001,
  B10001,
  B11111,
};
#define CHARBAR1 3

byte char_Bar2[8] = {
  B00100,
  B00100,
  B00100,
  B00111,
  B00000,
  B00000,
  B00000,
};
#define CHARBAR2 4

byte char_Bar3[8] = {
  B00100,
  B00100,
  B00100,
  B00100,
  B00100,
  B00100,
  B00100,
};
#define CHARBAR3 5

byte char_Bar4[8] = {
  B00100,
  B00100,
  B00100,
  B11100,
  B00000,
  B00000,
  B00000,
};
#define CHARBAR4 6


// Encoder

#define btnENC     10  // Comment if not using an encoder

#ifdef btnENC
#include <Rotary.h>
#define ENCODER_INTERRUPT_A 0
#define ENCODER_INTERRUPT_B 1
#define ENCODER_PIN_A 2
#define ENCODER_PIN_B 3
#define ENCODER_PIN_PUSH 15 // Use A1 as D15
volatile int encoder_position = 120;
int current_encoder_position = 120;
Rotary encoder = Rotary(ENCODER_PIN_A, ENCODER_PIN_B);
#endif
// =================================================================================

// Set pins used by the LCD panel
LiquidCrystal lcd(8, 9, 4, 5, 6, 7);
MIDI_CREATE_INSTANCE(HardwareSerial, Serial, MIDI);

// =================================================================================
// FUNCTIONS
// =================================================================================
const char custom[][8] PROGMEM = {                        // Custom character definitions
      { 0x1F, 0x1F, 0x1F, 0x00, 0x00, 0x00, 0x00, 0x00 }, // char 1 
      { 0x18, 0x1C, 0x1E, 0x1F, 0x1F, 0x1F, 0x1F, 0x1F }, // char 2 
      { 0x1F, 0x1F, 0x1F, 0x1F, 0x1F, 0x0F, 0x07, 0x03 }, // char 3 
      { 0x00, 0x00, 0x00, 0x00, 0x00, 0x1F, 0x1F, 0x1F }, // char 4 
      { 0x1F, 0x1F, 0x1F, 0x1F, 0x1F, 0x1E, 0x1C, 0x18 }, // char 5 
      { 0x1F, 0x1F, 0x1F, 0x00, 0x00, 0x00, 0x1F, 0x1F }, // char 6 
      { 0x1F, 0x00, 0x00, 0x00, 0x00, 0x1F, 0x1F, 0x1F }, // char 7 
      { 0x03, 0x07, 0x0F, 0x1F, 0x1F, 0x1F, 0x1F, 0x1F }  // char 8 
};

// BIG FONT Character Set
// - Each character coded as 1-4 byte sets {top_row(0), top_row(1)... bot_row(0), bot_row(0)..."}
// - number of bytes terminated with 0x00; Character is made from [number_of_bytes/2] wide, 2 high
// - codes are: 0x01-0x08 => custom characters, 0x20 => space, 0xFF => black square

const char bigChars[][8] PROGMEM = {
      { 0x20, 0x20, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 }, // Space
      { 0xFF, 0x07, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 }, // !
      { 0x05, 0x05, 0x20, 0x20, 0x00, 0x00, 0x00, 0x00 }, // "
      { 0x04, 0xFF, 0x04, 0xFF, 0x04, 0x01, 0xFF, 0x01 }, // #
      { 0x08, 0xFF, 0x06, 0x07, 0xFF, 0x05, 0x00, 0x00 }, // $
      { 0x01, 0x20, 0x04, 0x01, 0x04, 0x01, 0x20, 0x04 }, // %
      { 0x08, 0x06, 0x02, 0x20, 0x03, 0x07, 0x02, 0x04 }, // &
      { 0x05, 0x20, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 }, // '
      { 0x08, 0x01, 0x03, 0x04, 0x00, 0x00, 0x00, 0x00 }, // (
      { 0x01, 0x02, 0x04, 0x05, 0x00, 0x00, 0x00, 0x00 }, // )
      { 0x01, 0x04, 0x04, 0x01, 0x04, 0x01, 0x01, 0x04 }, // *
      { 0x04, 0xFF, 0x04, 0x01, 0xFF, 0x01, 0x00, 0x00 }, // +
      { 0x20, 0x05, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 }, // 
      { 0x04, 0x04, 0x04, 0x20, 0x20, 0x20, 0x00, 0x00 }, // -
      { 0x20, 0x04, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 }, // .
      { 0x20, 0x20, 0x04, 0x01, 0x04, 0x01, 0x20, 0x20 }, // /
      { 0x08, 0x01, 0x02, 0x03, 0x04, 0x05, 0x00, 0x00 }, // 0
      { 0x01, 0x02, 0x20, 0x04, 0xFF, 0x04, 0x00, 0x00 }, // 1
      { 0x06, 0x06, 0x02, 0xFF, 0x07, 0x07, 0x00, 0x00 }, // 2
      { 0x01, 0x06, 0x02, 0x04, 0x07, 0x05, 0x00, 0x00 }, // 3
      { 0x03, 0x04, 0xFF, 0x20, 0x20, 0xFF, 0x00, 0x00 }, // 4
      { 0xFF, 0x06, 0x06, 0x07, 0x07, 0x05, 0x00, 0x00 }, // 5
      { 0x08, 0x06, 0x06, 0x03, 0x07, 0x05, 0x00, 0x00 }, // 6
      { 0x01, 0x01, 0x02, 0x20, 0x08, 0x20, 0x00, 0x00 }, // 7
      { 0x08, 0x06, 0x02, 0x03, 0x07, 0x05, 0x00, 0x00 }, // 8
      { 0x08, 0x06, 0x02, 0x07, 0x07, 0x05, 0x00, 0x00 }, // 9
      { 0xA5, 0xA5, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 }, // :
      { 0x04, 0x05, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 }, // ;
      { 0x20, 0x04, 0x01, 0x01, 0x01, 0x04, 0x00, 0x00 }, // <
      { 0x04, 0x04, 0x04, 0x01, 0x01, 0x01, 0x00, 0x00 }, // =
      { 0x01, 0x04, 0x20, 0x04, 0x01, 0x01, 0x00, 0x00 }, // >
      { 0x01, 0x06, 0x02, 0x20, 0x07, 0x20, 0x00, 0x00 }, // ?
      { 0x08, 0x06, 0x02, 0x03, 0x04, 0x04, 0x00, 0x00 }, // @
      { 0x08, 0x06, 0x02, 0xFF, 0x20, 0xFF, 0x00, 0x00 }, // A
      { 0xFF, 0x06, 0x05, 0xFF, 0x07, 0x02, 0x00, 0x00 }, // B
      { 0x08, 0x01, 0x01, 0x03, 0x04, 0x04, 0x00, 0x00 }, // C
      { 0xFF, 0x01, 0x02, 0xFF, 0x04, 0x05, 0x00, 0x00 }, // D
      { 0xFF, 0x06, 0x06, 0xFF, 0x07, 0x07, 0x00, 0x00 }, // E
      { 0xFF, 0x06, 0x06, 0xFF, 0x20, 0x20, 0x00, 0x00 }, // F
      { 0x08, 0x01, 0x01, 0x03, 0x04, 0x02, 0x00, 0x00 }, // G
      { 0xFF, 0x04, 0xFF, 0xFF, 0x20, 0xFF, 0x00, 0x00 }, // H
      { 0x01, 0xFF, 0x01, 0x04, 0xFF, 0x04, 0x00, 0x00 }, // I
      { 0x20, 0x20, 0xFF, 0x04, 0x04, 0x05, 0x00, 0x00 }, // J
      { 0xFF, 0x04, 0x05, 0xFF, 0x20, 0x02, 0x00, 0x00 }, // K
      { 0xFF, 0x20, 0x20, 0xFF, 0x04, 0x04, 0x00, 0x00 }, // L
      { 0x08, 0x03, 0x05, 0x02, 0xFF, 0x20, 0x20, 0xFF }, // M
      { 0xFF, 0x02, 0x20, 0xFF, 0xFF, 0x20, 0x03, 0xFF }, // N
      { 0x08, 0x01, 0x02, 0x03, 0x04, 0x05, 0x00, 0x00 }, // 0
      { 0x08, 0x06, 0x02, 0xFF, 0x20, 0x20, 0x00, 0x00 }, // P
      { 0x08, 0x01, 0x02, 0x20, 0x03, 0x04, 0xFF, 0x04 }, // Q
      { 0xFF, 0x06, 0x02, 0xFF, 0x20, 0x02, 0x00, 0x00 }, // R
      { 0x08, 0x06, 0x06, 0x07, 0x07, 0x05, 0x00, 0x00 }, // S
      { 0x01, 0xFF, 0x01, 0x20, 0xFF, 0x20, 0x00, 0x00 }, // T
      { 0xFF, 0x20, 0xFF, 0x03, 0x04, 0x05, 0x00, 0x00 }, // U
      { 0x03, 0x20, 0x20, 0x05, 0x20, 0x02, 0x08, 0x20 }, // V
      { 0xFF, 0x20, 0x20, 0xFF, 0x03, 0x08, 0x02, 0x05 }, // W
      { 0x03, 0x04, 0x05, 0x08, 0x20, 0x02, 0x00, 0x00 }, // X
      { 0x03, 0x04, 0x05, 0x20, 0xFF, 0x20, 0x00, 0x00 }, // Y
      { 0x01, 0x06, 0x05, 0x08, 0x07, 0x04, 0x00, 0x00 }, // Z
      { 0xFF, 0x01, 0xFF, 0x04, 0x00, 0x00, 0x00, 0x00 }, // [
      { 0x01, 0x04, 0x20, 0x20, 0x20, 0x20, 0x01, 0x04 }, // Backslash
      { 0x01, 0xFF, 0x04, 0xFF, 0x00, 0x00, 0x00, 0x00 }, // ]
      { 0x08, 0x02, 0x20, 0x20, 0x00, 0x00, 0x00, 0x00 }, // ^
      { 0x20, 0x20, 0x20, 0x04, 0x04, 0x04, 0x00, 0x00 }  // _
};
byte col,row,nb=0,bc=0;                                   // general
byte bb[8];                                               // byte buffer for reading from PROGMEM
// writeBigChar: writes big character 'ch' to column x, row y; returns number of columns used by 'ch'
int writeBigChar(char ch, byte x, byte y) {
  if (ch < ' ' || ch > '_') return 0;               // If outside table range, do nothing
  nb=0;                                             // character byte counter 
  for (bc=0; bc<8; bc++) {                        
    bb[bc] = pgm_read_byte( &bigChars[ch-' '][bc] );  // read 8 bytes from PROGMEM
    if(bb[bc] != 0) nb++;
  }  
 
  bc=0;
  for (row = y; row < y+2; row++) {
    for (col = x; col < x+nb/2; col++ ) {
      lcd.setCursor(col, row);                      // move to position
      lcd.write(bb[bc++]);                          // write byte and increment to next
    }
//    lcd.setCursor(col, row);
//    lcd.write(' ');                                 // Write ' ' between letters
  }
  return nb/2-1;                                      // returns number of columns used by char
}

// writeBigString: writes out each letter of string
void writeBigString(char *str, byte x, byte y) {
  char c;
  while ((c = *str++))
  x += writeBigChar(c, x, y) + 1;
}



// =================================================================================
// Encoder management

#ifdef btnENC
bool encoderPositionUpdated() {
  static int last_position = -999;

  // disable interrupts while we copy the current encoder state
  uint8_t old_SREG = SREG;
  cli();

  if ( encoder_position > 300 ) encoder_position = 300;
  else if ( encoder_position < 30 ) encoder_position = 30; 
  current_encoder_position = encoder_position;

  SREG = old_SREG;

  bool updated = (current_encoder_position != last_position);
  last_position = current_encoder_position;

  return updated;
}
                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                   /*
  Interrupt Service Routine:
  reads the encoder on pin A or B change
 */
void loadEncoderPositionOnChange() {
  unsigned char result = encoder.process();
  if (result == DIR_NONE) {
    // do nothing
  }
  else if (result == DIR_CW) {
  
    encoder_position++;
 
  }
  else if (result == DIR_CCW) {

    encoder_position--;

  }
}
#endif

// =================================================================================
// Keypad reliable reading
// As many keypad shields use a resistor network on analog pin 0, 
// unreliable results are seen when pressing buttons.
// This function acts as a filter and returns value after 6 consistents reads or -1 after 20 try
int keypadRead(byte pin) {
    int adcKeyPressed ;
    byte i=0;

    if ( (adcKeyPressed = analogRead(pin))  > 1000) return adcKeyPressed;
    while ( (adcKeyPressed = analogRead(pin)) - analogRead(pin)  + analogRead(pin) - analogRead(pin) + analogRead(pin) - analogRead(pin) ) {   
      if ( i++ > 20 ) return -1;
    }
    return adcKeyPressed; 
}

// Read the buttons
// Note that values have been ajusted comparing to examples, notably the btnRight range.
int readButtons()
{ 
 int adcKeyPressed;

#ifdef btnENC
 
 // Push mode of encoder. Same function used for more convenience.
 // Encoder is processed first.
 adcKeyPressed = digitalRead(ENCODER_PIN_PUSH);    
 if (adcKeyPressed ==0) return btnENC;
 
#endif
 
 // Keypad buttons
 if ( (adcKeyPressed = keypadRead(0)) >=0 ) {    
     if (adcKeyPressed > 1000) return btnRELEASED; // 1023
     if (adcKeyPressed < 120)  return btnRIGHT;    // 0
     if (adcKeyPressed < 250)  return btnUP;       // 130
     if (adcKeyPressed < 450)  return btnDOWN;     // 305
     if (adcKeyPressed < 650)  return btnLEFT;     // 478
     if (adcKeyPressed < 850)  return btnSELECT;   // 720
 }
 return -1;  // when all others fail, return this...
}

// Process buttons
int processButtons()
{
 static int lastAdcKeyPressed = btnRELEASED;
 static boolean holded = false;
 static unsigned long lastDebounceKeyTime=0; 
 int adcKeyPressed=0, retKeyPressed=0;

 if ( (adcKeyPressed = readButtons()) < 0) return -1; 
 
 // Check if a key was released or holded
 if ( adcKeyPressed == btnRELEASED ) {     
    if (lastAdcKeyPressed != btnRELEASED ) {           
        retKeyPressed = lastAdcKeyPressed;
        lastAdcKeyPressed = adcKeyPressed;
        if (holded) retKeyPressed+= 100;
        lastDebounceKeyTime = 0;
        holded = false;
        return retKeyPressed;
    }     
 } else if ( adcKeyPressed == lastAdcKeyPressed ) {
    if ( lastDebounceKeyTime == 0 ) lastDebounceKeyTime = millis();
    else if ( (millis() - lastDebounceKeyTime) > debounceDelay ){
        // Keyholded
        lastDebounceKeyTime = 0;
        lastAdcKeyPressed = adcKeyPressed;
        holded = true;
        return adcKeyPressed + 1000;
    }
 }

 lastAdcKeyPressed = adcKeyPressed;
 return -1;
}
 

// =================================================================================
// Midi clock tick. Called 24 times per quarter notes.

#define CVGATE_DIVIDER 12  // PO sync pulse is a 1Vpp square pulse with a little less than 3ms
//#define CVGATE_DIVIDER 6

void midiClockTick(){                                 
  static byte clockCounter=0;
  static byte cvGateDiviser=CVGATE_DIVIDER; 
  
  // Always send the midi clock
  MIDI.sendRealTime(midi::Clock);

  // Start trigger
  if ( sendStart)  {
     lcd.setCursor(15,0);
     if (isPlaying) {
        MIDI.sendRealTime(midi::Stop);
        lcd.write(byte(CHARPAUSE));
        isPlaying=false; isPaused=true;     
     } else {      
        if ( isPaused ) {
            isPaused = false;
            MIDI.sendRealTime(midi::Continue);                                      
        } else {
          clockCounter = 0; // reset counters
          songPointerPos = 0;
          cvGateDiviser=CVGATE_DIVIDER;
          MIDI.sendSongPosition(songPointerPos);
          MIDI.sendRealTime(midi::Start);
        }
        lcd.write(byte(CHARPLAY));
        isPlaying = true;             
     }
     sendStart = false;
  }

  if (isPlaying) {
    clockCounter++; 
    
    // CV Gate
    if ( cvGateDiviser-- == CVGATE_DIVIDER  ) {
            CVPulse.start();
    } else if ( cvGateDiviser ==0) cvGateDiviser=CVGATE_DIVIDER;    
    
    // Midi
    if (clockCounter == 1) {      
        showSongPos();
    } 
    else if (clockCounter == 6 )  {  songPointerPos++; showSongPos();}
    else if (clockCounter == 12 ) {  songPointerPos++; showSongPos();}  
    else if (clockCounter == 18 ) {  songPointerPos++; showSongPos();}  
    else if (clockCounter >= 24 ) {  clockCounter = 0 ; songPointerPos++; showSongPos();}   
  }
  // Resync mode
  if (sendResync) {
//    songPointerPos = 0;
      MIDI.sendSongPosition(songPointerPos);
      //MIDI.sendRealTime(midi::Start);
      sendResync = false;
  }
}

// =================================================================================
// FREERAM: Returns the number of bytes currently free in RAM  
int freeRAM(void) {
  extern int  __bss_end, *__brkval; 
  int free_memory; 
  if((int)__brkval == 0) {
    free_memory = ((int)&free_memory) - ((int)&__bss_end); 
  } 
  else {
    free_memory = ((int)&free_memory) - ((int)__brkval); 
  }
  return free_memory; 
}

void showFreeRam() {
 lcd.clear();
 lcd.print("KloK | Free RAM:");
 lcd.setCursor(0,1);
 lcd.print(freeRAM());
}

void showBuild() {
 lcd.clear();
 lcd.print("KloK | Build:   ");
 lcd.setCursor(0,1);
 lcd.print(TimestampedVersion); 
}

void showWelcome() {
 showBuild();
 delay(3000);
 showFreeRam();
 delay(2000);
 lcd.clear();
 lcd.print(F("KloK "));
 
 showBPM();
 lcd.setCursor(15,0);
 lcd.write(byte(CHARSTOP));
 lcd.setCursor(0,1);
 lcd.print("Press PLAY>");
}
void showSongPos() {

 lcd.setCursor(14,0);
 lcd.write(byte(CHARBAR1+songPointerPos%16/4) );
 
 lcd.setCursor(6,1);
 lcd.print("     ");
 lcd.setCursor(0,1);
 if (songPointerPos) {
   lcd.print(songPointerPos/16+1);
   lcd.print(":");
   lcd.print(songPointerPos%16/4+1);
   lcd.print(":");
   byte p = songPointerPos%16+1;
   if (p<10) lcd.print("0");
   lcd.print(p);
   //lcd.print(songPointerPos);
 } else lcd.print("1:1:01"); 
 
}

void showBPM() {
 lcd.setCursor(11,1);
 if (bpm < 100) lcd.print(" ");
 lcd.print(bpm,1);
}

void midiSendAllNotesOff(int c=0) { 
  if (c < 1 || c >16 ) {
    for (byte i=1; i<= 16; i++ ) MIDI.sendControlChange(123,0,i);
  } else MIDI.sendControlChange(123,0,c);
}

// =================================================================================
// START HERE
// =================================================================================
void setup()   {                
  
 //pinMode(LED, OUTPUT);
 //pinMode(CVGATE_PIN,OUTPUT); // CV Gate PIN//
 
 // Initialize micros counters
 clockTick = 60000000.0/bpm/24.0;
 oldMicros = clockTick;

#ifdef btnENC 
 // Init encoder
 encoderPositionUpdated();
 attachInterrupt(ENCODER_INTERRUPT_A, loadEncoderPositionOnChange, CHANGE);
 attachInterrupt(ENCODER_INTERRUPT_B, loadEncoderPositionOnChange, CHANGE);
#endif
 
 // Start LCD library
 lcd.begin(16, 2); 


  for (nb=0; nb<8; nb++ ) {                     // create 8 custom characters
    for (bc=0; bc<8; bc++) bb[bc]= pgm_read_byte( &custom[nb][bc] );
    lcd.createChar ( nb+1, bb );
  }
  
  lcd.clear();
  writeBigString("  KLOK", 0, 0);
  delay(5000);
  
 // Prepare specials LCD characters 
 lcd.createChar(CHARPLAY, char_Play);  lcd.createChar(CHARPAUSE, char_Pause);  lcd.createChar(CHARSTOP, char_Stop);  
 lcd.createChar(CHARBAR1, char_Bar1);  lcd.createChar(CHARBAR2, char_Bar2);  
 lcd.createChar(CHARBAR3, char_Bar3);  lcd.createChar(CHARBAR4, char_Bar4);

  
 // Init MIDI
  MIDI.begin(MIDI_CHANNEL_OMNI);
  MIDI.turnThruOn(midi::Thru::Full); // Off but system messages. ALWAYS AFTER BEGIN ELSE IT DOESN'T WORK !!
/*
    Off                   = 0,  ///< Thru disabled (nothing passes through).
    Full                  = 1,  ///< Fully enabled Thru (every incoming message is sent back).
    SameChannel           = 2,  ///< Only the messages on the Input Channel will be sent back.
    DifferentChannel      = 3,  ///< All the messages but the ones on the Input Channel will be sent back.
};*/

 MIDI.sendRealTime(midi::Stop);
 MIDI.sendSongPosition(songPointerPos); 
 midiSendAllNotesOff();

 // PulseOut init
 CVPulse.begin();
 
 // Display Welcome screen
  
 showWelcome(); 
}



// =================================================================================
// LOOP
void loop() {
    boolean refreshDisplay = false;
    boolean restartBPM = false;
    int button  = 0;

    CVPulse.update(millis());

    // I do not use a timer for the best accuracy possible in microsecs
    newMicros = micros() ;   
    if ( newMicros - oldMicros  > (unsigned long) clockTick ) {
       midiClockTick();
       oldMicros += clockTick;       
    }
    
#ifdef btnENC
 
    if(encoderPositionUpdated()) { 
          bpm = current_encoder_position;
          refreshDisplay = true; 
          restartBPM = true;
    }
#endif
    
    // read MIDI events
    //MIDI.read(); 

    // read Buttons
    if ( (button = processButtons()) != -1 )
    {      
      switch (button)   
      {      
        case btnPLAY:
#ifdef btnENC
        case btnENC:  
#endif
        { 
            // Asynchroneous trigger
            sendStart = true;                               
            break;                    
        }      
        case btnSTOP:
#ifdef btnENC         
        case btnENC+1000:
#endif 
        {
            MIDI.sendRealTime(midi::Stop);
            midiSendAllNotesOff();
            songPointerPos=0;
            MIDI.sendSongPosition(songPointerPos); 
            lcd.setCursor(14,0);
            lcd.print(' ');
            lcd.write(byte(CHARSTOP));         
            refreshDisplay = true;
            isPlaying = isPaused = false;
            break;                    
        }
        case btnBPM_INC: { 
            bpm += 0.1;
            if ( bpm > 300 ) bpm = 30; 
            refreshDisplay = true; 
            restartBPM = true;
            break;        
        }
        case btnBPM_DEC:  { 
            bpm -= 0.1;         
            if ( bpm < 30 ) bpm = 300; 
            refreshDisplay = true; 
            restartBPM = true;
            break;
        }
    
        case btnBPM_INC+1000: { 
            if ( (bpm+=10) > 300 ) bpm = 30; 
            refreshDisplay = true;
            restartBPM = true;
            break;        
        }
    
        case btnBPM_DEC+1000: { 
           if ( (bpm-=10) < 30 ) bpm = 300; 
           refreshDisplay = true;        
           restartBPM = true;
           break;
        }
    
        case btnMODE+1000: { 
          //if (isPlaying)
          sendResync = true;
          lcd.setCursor(12,0);
          lcd.print("S");        
          break; 
        }
      }
                                         
  } 
  if (restartBPM) {
      // ClockTick in microsecs
      clockTick = 60000000.0/bpm/24.0 ;
      restartBPM=false;      
  }
  
  if (refreshDisplay) {
     showBPM();
     showSongPos();
     refreshDisplay=false;
  }     
}


