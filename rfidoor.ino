#include <Wire.h>
#include <SPI.h>
#include <Adafruit_PN532.h>
#include"pitches.h"

// If using the breakout or shield with I2C, define just the pins connected
// to the IRQ and reset lines.  Use the values below (2, 3) for the shield!
#define PN532_IRQ   (2)
#define PN532_RESET (3)  // Not connected by default on the NFC Shield
Adafruit_PN532 nfc(PN532_IRQ, PN532_RESET);

//--------------------------------------------------------------------------
// !!!KEYS!!!
// This is where you define what keys will be authorized.
// Written as byte arrays, sample empty 4 and 7 byte keys are written below.
// It's also a good idea to track who each key is issued to in case of
// loss, theft or revocation.

uint8_t authenticated4ByteUids[][4] = {
  {0x00, 0x00, 0x00, 0x00}, // Keyholder Name
};
uint8_t authenticated7ByteUids[][7] = {
  {0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00} // Keyholder Name
};
//--------------------------------------------------------------------------

// pins
const int hall = 10;
const int piezo = 11;
const int redLed = 12;
const int greenLed = 13;
const int relayDoor = 7;
const int relayLight = 6;

// constants
int attemptCount = 0;
int attemptMax = 5;
int attemptTimeout = 5000;
unsigned long attempStrikeOutTimeout = 1000L*60L*5L;

// variables
int hallState = 0;
boolean isAuthenticated = false;
volatile byte doorState = LOW; // LOW|closed HIGH|open

// tunes
int successMelody[] = {
  NOTE_E7, NOTE_E7, 0, NOTE_E7,
  0, NOTE_C7, NOTE_E7, 0,
  NOTE_G7, 0, 0, 0,
  NOTE_G6, 0, 0, 0,
 
  NOTE_C7, 0, 0, NOTE_G6,
  0, 0, NOTE_E6, 0,
  0, NOTE_A6, 0, NOTE_B6,
  0, NOTE_AS6, NOTE_A6, 0,
 
//  NOTE_G6, NOTE_E7, NOTE_G7,
//  NOTE_A7, 0, NOTE_F7, NOTE_G7,
//  0, NOTE_E7, 0, NOTE_C7,
//  NOTE_D7, NOTE_B6, 0, 0,
 
//  NOTE_C7, 0, 0, NOTE_G6,
//  0, 0, NOTE_E6, 0,
//  0, NOTE_A6, 0, NOTE_B6,
//  0, NOTE_AS6, NOTE_A6, 0,
// 
//  NOTE_G6, NOTE_E7, NOTE_G7,
//  NOTE_A7, 0, NOTE_F7, NOTE_G7,
//  0, NOTE_E7, 0, NOTE_C7,
//  NOTE_D7, NOTE_B6, 0, 0
};

//Mario main them tempo
int successTempo[] = {
  12, 12, 12, 12,
  12, 12, 12, 12,
  12, 12, 12, 12,
  12, 12, 12, 12,
 
  12, 12, 12, 12,
  12, 12, 12, 12,
  12, 12, 12, 12,
  12, 12, 12, 12,
 
//  9, 9, 9,
//  12, 12, 12, 12,
//  12, 12, 12, 12,
//  12, 12, 12, 12,
 
//  12, 12, 12, 12,
//  12, 12, 12, 12,
//  12, 12, 12, 12,
//  12, 12, 12, 12,
// 
//  9, 9, 9,
//  12, 12, 12, 12,
//  12, 12, 12, 12,
//  12, 12, 12, 12,
};

/*  _   _                       _
   / \_/ \_   _   _   _   _   _/ \_
   \_/ \_/ \_/ \_/ \_/ \_/ \_/ \_/ \
   / \_/ \_/ \_/ \_/ \_/ \_/ \_/ \_/
   \_/ \_/ \_/ \_/ \_/ \_/ \_/
             \_/ \_/ \_/
*/
void setup(void) {
  pinMode(piezo, OUTPUT);
  pinMode(redLed, OUTPUT);
  pinMode(greenLed, OUTPUT);
  pinMode(relayDoor, OUTPUT);
  pinMode(relayLight, OUTPUT);
  pinMode(hall, INPUT_PULLUP);
  attachInterrupt(digitalPinToInterrupt(hall), isDoorOpen, CHANGE);
  digitalWrite(relayDoor, HIGH);
  digitalWrite(relayLight, HIGH);
  Serial.begin(115200);
  Serial.println("Hello!");

  nfc.begin();

  uint32_t versiondata = nfc.getFirmwareVersion();
  if (!versiondata) {
    //Serial.print("Didn't find PN53x board");
    while (1); // halt
  }
  // Got ok data, print it out!
  //Serial.print("Found chip PN5"); Serial.println((versiondata>>24) & 0xFF, HEX); 
  //Serial.print("Firmware ver. "); Serial.print((versiondata>>16) & 0xFF, DEC); 
  //Serial.print('.'); Serial.println((versiondata>>8) & 0xFF, DEC);
  
  // configure board to read RFID tags
  nfc.SAMConfig();
  
  //Serial.println("Waiting for an ISO14443A Card ...");
}


/*  _   _                       _
   / \_/ \_   _   _   _   _   _/ \_
   \_/ \_/ \_/ \_/ \_/ \_/ \_/ \_/ \
   / \_/ \_/ \_/ \_/ \_/ \_/ \_/ \_/
   \_/ \_/ \_/ \_/ \_/ \_/ \_/
             \_/ \_/ \_/
*/
void loop(void) {  
  uint8_t success;
  uint8_t uid[] = { 0, 0, 0, 0, 0, 0, 0 };  // Buffer to store the returned UID
  uint8_t uidLength;                        // Length of the UID (4 or 7 bytes depending on ISO14443A card type)
    
  // Wait for an ISO14443A type cards (Mifare, etc.).  When one is found
  // 'uid' will be populated with the UID, and uidLength will indicate
  // if the uid is 4 bytes (Mifare Classic) or 7 bytes (Mifare Ultralight)
  success = nfc.readPassiveTargetID(PN532_MIFARE_ISO14443A, uid, &uidLength);
  
  if (success) {
    // Display some basic information about the card
    //Serial.println("Found an ISO14443A card");
    //Serial.print("  UID Length: ");Serial.print(uidLength, DEC);Serial.println(" bytes");
    //Serial.print("  UID Value: ");
    //nfc.PrintHex(uid, uidLength);
    //Serial.println("");

    bool isAuthenticated = false;    
    if (uidLength == 4) {
      isAuthenticated = handle4byte(uid);
    }
    else if (uidLength == 7) {
      isAuthenticated = handle7byte(uid);
    }

    if (isAuthenticated) {
      openSesami();
    }
    else {
      strike();
    }
  }
  Serial.flush();
}

bool handle4byte(uint8_t uid[]) {
  //Serial.println("Seems to be a Mifare Classic card (4 byte UID)");

  // Now we need to try to authenticate it for read/write access
  // Try with the factory default KeyA: 0xFF 0xFF 0xFF 0xFF 0xFF 0xFF
  //Serial.println("Trying to authenticate block 4 with default KEYA value");
  uint8_t keya[6] = { 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF };
  uint8_t success = nfc.mifareclassic_AuthenticateBlock(uid, 4, 4, 0, keya);

  if (success){
    //Serial.println("Sector 1 (Blocks 4..7) has been authenticated");
    uint8_t data[16];

    // If you want to write something to block 4, uncomment
    //memcpy(data, (const uint8_t[]){ 'S', 'a', 'm', 'p', 'l', 'e', ' ', 'K', 'e', 'y', 0, 0, 0, 0, 0, 0 }, sizeof data);    
    //success = nfc.mifareclassic_WriteDataBlock (4, data);
    
    success = nfc.mifareclassic_ReadDataBlock(4, data);    
    if (success){
      //Serial.println("Reading Block 4:");
      nfc.PrintHexChar(data, 16);
      //Serial.println("");
    }
  }

  int keyCount4Byte = sizeof(authenticated4ByteUids)/4;
  for (int i=0; i < keyCount4Byte; i++){
    bool authenticated = true;
    for (int j=0; j < 4; j++) {
      if (uid[j] != authenticated4ByteUids[i][j]) {
        authenticated = false;
      }
    }
    if (authenticated) {
      return true;
    }
  }
  return false;
}

bool handle7byte(uint8_t uid[]) {
  //Serial.println("Seems to be a Mifare Ultralight tag (7 byte UID)");
  //Serial.println("Reading page 4");
  uint8_t data[32];
  uint8_t success = nfc.mifareultralight_ReadPage (4, data);
  if (success){
    nfc.PrintHexChar(data, 4);
    //Serial.println("");
  }
  
  int keyCount7Byte = sizeof(authenticated7ByteUids)/7;
  for (int i=0; i < keyCount7Byte; i++){
    bool authenticated = true;
    for (int j=0; j < 7; j++) {
      if (uid[j] != authenticated7ByteUids[i][j]) {
        authenticated = false;
      }
    }
    if (authenticated) {
      return true;
    }
  }
  return false;
}

void letThereBeLight() {
  Serial.print("Let there be light ~(o.o)~");
  digitalWrite(relayLight, LOW);
}

void inTheDarknessBind() {
  Serial.print("In the darkness, bind them `(-,-`)");
  digitalWrite(relayLight, HIGH);
}

void openSesami() {
  //Serial.print("Open Sesami t(^.^t)");
  isAuthenticated = true;
  attemptCount = 0;
  digitalWrite(relayDoor, LOW);
  playSuccessSong();
  digitalWrite(relayDoor, HIGH);
  isAuthenticated = false;
}

void strike() {
  //Serial.print("Strike t(*^*t)");
  playFailSong();
  attemptCount++;
  if (attemptCount >= attemptMax) {
    //Serial.print("STRIKE OUT! t(@O@t)");
    delay(attempStrikeOutTimeout);
  }
  else{
    delay(attemptTimeout);
  }
}

// t(^.^t)
void playSuccessSong() {
  //digitalWrite(greenLed, HIGH);
  int duration = 800; //1000;
  int pause = 1.25; //1.30;  
  int size = sizeof(successMelody) / sizeof(int);
  for (int thisNote = 0; thisNote < size; thisNote++) {
    int noteDuration = duration / successTempo[thisNote];
    tone(piezo, successMelody[thisNote],noteDuration);
    int pauseBetweenNotes = noteDuration * pause;
    delay(pauseBetweenNotes);
    noTone(piezo);
    delay(noteDuration);
  }
  //digitalWrite(greenLed, LOW);
}

// t(*^*t)
void playFailSong() {
  //digitalWrite(redLed, HIGH);
  tone(piezo,NOTE_G4);
  delay(250);
  tone(piezo,NOTE_C4);
  delay(500);
  noTone(piezo);
  //digitalWrite(redLed, LOW);
}

// (((O)))
void isDoorOpen() {
  doorState = digitalRead(hall); // LOW|closed HIGH|open
  Serial.println("------------------------------------");
  Serial.println(doorState);
  if (doorState == LOW) { // closed
    // turn off lights
    Serial.println("Lights OFF!");
    digitalWrite(greenLed, LOW);
    inTheDarknessBind();
  }
  else { // open
    if (!isAuthenticated) {
      // playAlarm
      Serial.println("ALARM!");
    }
    else {
      // turn on lights
      Serial.println("Lights ON!");
      digitalWrite(greenLed, HIGH);
      letThereBeLight();
    }
  }
}



