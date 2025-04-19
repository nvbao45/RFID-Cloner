// This file implements an RFID Cloner using an MFRC522 RFID module and an Nokia 5110 LCD display.
// It allows switching between read and write modes using a button and displays the current mode and status on the LCD.
// Pin diagram:
// MFRC522:
//   - RST_PIN: 9
//   - SS_PIN: 10
// Nokia 5110 LCD:
//   - CLK: 7
//   - DIN: 5
//   - DC: 4
//   - RST: 3
//   - CS: 2
// Button:
//   - BUTTON_PIN: 8
// LCD Backlight:
//   - LCD_BL_PIN: 6


#include <SPI.h>
#include <MFRC522.h>
#include <LCD5110_Graph.h>

// RFID RC522 Pins
#define RST_PIN 9
#define SS_PIN 10
MFRC522 mfrc522(SS_PIN, RST_PIN);

// LCD5110 Pins: CLK, DIN, DC, RST, CS
LCD5110 myGLCD(7, 5, 4, 3, 2);
extern uint8_t SmallFont[];

#define BUTTON_PIN 8        // Button to switch modes
#define LCD_BL_PIN 6        // LCD Backlight control pin

bool isWriteMode = false;
unsigned long lastButtonPress = 0;

// Storage
byte savedUID[4];
byte savedBlock[16];
bool hasSavedData = false;

void setup() {
  Serial.begin(9600);
  while (!Serial);

  pinMode(LCD_BL_PIN, OUTPUT);
  digitalWrite(LCD_BL_PIN, HIGH);

  myGLCD.InitLCD();
  myGLCD.setFont(SmallFont);
  myGLCD.print("Initializing...", CENTER, 0);
  myGLCD.update();

  SPI.begin();
  mfrc522.PCD_Init();

  pinMode(BUTTON_PIN, INPUT_PULLUP);

  delay(1000);
  myGLCD.clrScr();
  showMode();
  Serial.println("[INFO] RFID Cloner Ready.");
}

void loop() {
  handleModeButton();

  if (!mfrc522.PICC_IsNewCardPresent() || !mfrc522.PICC_ReadCardSerial())
    return;

  digitalWrite(LCD_BL_PIN, HIGH);

  byte *uid = mfrc522.uid.uidByte;
  byte uidSize = mfrc522.uid.size;

  myGLCD.clrScr();
  myGLCD.print(isWriteMode ? "Write Mode" : "Read Mode", CENTER, 0);
  myGLCD.print("UID:", LEFT, 12);

  char uidBuf[20];
  sprintf(uidBuf, "%02X %02X %02X %02X", uid[0], uid[1], uid[2], uid[3]);
  myGLCD.print(uidBuf, LEFT, 22);
  Serial.print("[UID] "); Serial.println(uidBuf);

  if (!isWriteMode) {
    readBlock1(uid);
  } else {
    writeBlock1(uid);
  }

  myGLCD.update();
  delay(2000);
  digitalWrite(LCD_BL_PIN, LOW);

  mfrc522.PICC_HaltA();
  mfrc522.PCD_StopCrypto1();
}

void handleModeButton() {
  if (digitalRead(BUTTON_PIN) == LOW && millis() - lastButtonPress > 500) {
    isWriteMode = !isWriteMode;

    Serial.print("[MODE] Now in ");
    Serial.println(isWriteMode ? "WRITE MODE" : "READ MODE");

    showMode();
    lastButtonPress = millis();
    delay(100);
  }
}

void showMode() {
  digitalWrite(LCD_BL_PIN, HIGH);
  myGLCD.clrScr();
  myGLCD.print("RFID Cloner", CENTER, 0);
  myGLCD.print(isWriteMode ? "Write Mode" : "Read Mode", CENTER, 20);
  myGLCD.update();
}

void readBlock1(byte *uid) {
  MFRC522::StatusCode status;
  byte block = 1;
  byte buffer[18];
  byte size = sizeof(buffer);

  MFRC522::MIFARE_Key key = {{0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF}};

  status = mfrc522.PCD_Authenticate(MFRC522::PICC_CMD_MF_AUTH_KEY_A, block, &key, &(mfrc522.uid));
  if (status != MFRC522::STATUS_OK) {
    myGLCD.print("Auth Fail", LEFT, 34);
    Serial.println("[ERROR] Authentication failed");
    return;
  }

  status = mfrc522.MIFARE_Read(block, buffer, &size);
  if (status == MFRC522::STATUS_OK) {
    memcpy(savedUID, uid, 4);
    memcpy(savedBlock, buffer, 16);
    hasSavedData = true;
    myGLCD.print("Block Saved", LEFT, 34);
    Serial.println("[INFO] Block 1 saved.");
  } else {
    myGLCD.print("Read Fail", LEFT, 34);
    Serial.println("[ERROR] Block read failed.");
  }
}

void writeBlock1(byte *uid) {
  if (!hasSavedData) {
    myGLCD.print("No Data!", LEFT, 34);
    Serial.println("[WARN] No saved block to write.");
    return;
  }

  if (memcmp(savedUID, uid, 4) == 0) {
    myGLCD.print("Same Card!", LEFT, 34);
    Serial.println("[WARN] Same card detected. Skipping write.");
    return;
  }

  delay(100);
  mfrc522.PCD_StopCrypto1();

  MFRC522::StatusCode status;
  byte block = 1;
  MFRC522::MIFARE_Key key = {{0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF}};

  status = mfrc522.PCD_Authenticate(MFRC522::PICC_CMD_MF_AUTH_KEY_A, block, &key, &(mfrc522.uid));
  if (status != MFRC522::STATUS_OK) {
    myGLCD.print("Auth Fail", LEFT, 34);
    Serial.println("[ERROR] Auth failed on write.");
    return;
  }

  if (mfrc522.MIFARE_SetUid(savedUID, (byte)4, true)) {
    myGLCD.print("UID Set", LEFT, 34);
    Serial.println("[INFO] UID written successfully.");
  } else {
    myGLCD.print("Write Fail", LEFT, 34);
    Serial.println("[ERROR] Write failed.");
  }

  mfrc522.PCD_StopCrypto1();
}
