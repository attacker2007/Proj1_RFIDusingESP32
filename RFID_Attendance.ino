#include <SPI.h>
#include <MFRC522.h>

// ===================== PIN CONFIG =====================
// RC522 (SPI pins - ESP32 VSPI)
#define SS_PIN   21
#define RST_PIN  4
#define SCK_PIN  18
#define MISO_PIN 19
#define MOSI_PIN 23

// Buzzer
#define BUZZER_PIN 25

MFRC522 rfid(SS_PIN, RST_PIN);

// ===================== SETTINGS =====================
const unsigned long SCAN_DELAY = 2000; // prevent spam scans (ms)
unsigned long lastScanTime = 0;

// ===================== BUZZER =====================
void beep(int duration) {
  digitalWrite(BUZZER_PIN, HIGH);
  delay(duration);
  digitalWrite(BUZZER_PIN, LOW);
}

// ===================== UID PRINT =====================
void printUID(byte *uid, byte size) {
  for (byte i = 0; i < size; i++) {
    if (uid[i] < 0x10) Serial.print("0");
    Serial.print(uid[i], HEX);
    Serial.print(" ");
  }
}

// ===================== ATTENDANCE LOG =====================
void logAttendance(byte *uid, byte size) {
  Serial.print("TIME(ms): ");
  Serial.print(millis());

  Serial.print(" | UID: ");
  printUID(uid, size);

  Serial.println();
}

// ===================== SETUP =====================
void setup() {
  Serial.begin(115200);
  delay(1000);

  // SPI init (important for ESP32)
  SPI.begin(SCK_PIN, MISO_PIN, MOSI_PIN, SS_PIN);

  rfid.PCD_Init();

  pinMode(BUZZER_PIN, OUTPUT);
  digitalWrite(BUZZER_PIN, LOW);

  Serial.println("RFID Attendance System Ready...");
  beep(1000);

}

// ===================== LOOP =====================
void loop() {

  // Prevent rapid repeated scans
  if (millis() - lastScanTime < SCAN_DELAY) {
    return;
  }

  // Check for card
  if (!rfid.PICC_IsNewCardPresent()) {
    return;
  }

  if (!rfid.PICC_ReadCardSerial()) {
    return;
  }

  lastScanTime = millis();

  Serial.println("------------------------");
  Serial.println("Card Detected!");

  // Log attendance
  logAttendance(rfid.uid.uidByte, rfid.uid.size);

  // Buzzer feedback
  beep(200);

  // Optional: extra confirmation beep pattern
  delay(100);
  beep(100);

  // Stop communication with card
  rfid.PICC_HaltA();
  rfid.PCD_StopCrypto1();
}