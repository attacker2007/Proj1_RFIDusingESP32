#include <SPI.h>
#include <MFRC522.h>

// ===================== PIN CONFIG =====================
#define SS_PIN    21
#define RST_PIN   4
#define SCK_PIN   18
#define MISO_PIN  19
#define MOSI_PIN  23
#define BUZZER_PIN 25

MFRC522 rfid(SS_PIN, RST_PIN);

// ===================== SETTINGS =====================
const unsigned long SCAN_COOLDOWN  = 2000;   // Anti-spam: min ms between any scans
const unsigned long MIN_CHECKOUT   = 10000;  // ⬅ Minimum ms between TIME IN and TIME OUT (change this)
const int MAX_CARDS = 5;

// ===================== CARD STATE =====================
struct CardRecord {
  byte uid[10];
  byte uidSize;
  bool isCheckedIn;
  unsigned long timeInMs;
  bool active;  // true = this slot is occupied
};

CardRecord cards[MAX_CARDS];
unsigned long lastScanTime = 0;

// ===================== HELPERS =====================
bool uidMatch(byte *a, byte sizeA, byte *b, byte sizeB) {
  if (sizeA != sizeB) return false;
  for (byte i = 0; i < sizeA; i++) {
    if (a[i] != b[i]) return false;
  }
  return true;
}

void printUID(byte *uid, byte size) {
  for (byte i = 0; i < size; i++) {
    if (uid[i] < 0x10) Serial.print("0");
    Serial.print(uid[i], HEX);
    if (i < size - 1) Serial.print(":");
  }
}

// Format milliseconds -> "Xh Ym Zs"
String formatDuration(unsigned long ms) {
  unsigned long totalSec = ms / 1000;
  unsigned long hours    = totalSec / 3600;
  unsigned long minutes  = (totalSec % 3600) / 60;
  unsigned long seconds  = totalSec % 60;

  String result = "";
  if (hours > 0)   result += String(hours)   + "h ";
  if (minutes > 0) result += String(minutes) + "m ";
  result += String(seconds) + "s";
  return result;
}

// Format raw ms timestamp -> "HH:MM:SS (Xms)"
String formatTimestamp(unsigned long ms) {
  unsigned long totalSec = ms / 1000;
  unsigned long hh = totalSec / 3600;
  unsigned long mm = (totalSec % 3600) / 60;
  unsigned long ss = totalSec % 60;

  char buf[32];
  snprintf(buf, sizeof(buf), "%02lu:%02lu:%02lu (uptime %lums)", hh, mm, ss, ms);
  return String(buf);
}

// ===================== BUZZER =====================
void beep(int duration) {
  digitalWrite(BUZZER_PIN, HIGH);
  delay(duration);
  digitalWrite(BUZZER_PIN, LOW);
}

void beepSuccess() {  // Two short beeps = TIME IN
  beep(150); delay(80); beep(150);
}

void beepCheckout() { // One long beep = TIME OUT
  beep(600);
}

void beepRejected() { // Three rapid beeps = too soon
  beep(80); delay(60); beep(80); delay(60); beep(80);
}

// ===================== CARD LOOKUP / REGISTER =====================
int findCard(byte *uid, byte size) {
  for (int i = 0; i < MAX_CARDS; i++) {
    if (cards[i].active && uidMatch(cards[i].uid, cards[i].uidSize, uid, size)) {
      return i;
    }
  }
  return -1;
}

int registerCard(byte *uid, byte size) {
  for (int i = 0; i < MAX_CARDS; i++) {
    if (!cards[i].active) {
      memcpy(cards[i].uid, uid, size);
      cards[i].uidSize    = size;
      cards[i].isCheckedIn = false;
      cards[i].timeInMs   = 0;
      cards[i].active     = true;
      return i;
    }
  }
  return -1;  // No free slots
}

// ===================== ATTENDANCE LOGIC =====================
void handleCard(byte *uid, byte size) {
  int idx = findCard(uid, size);

  // First time seeing this card — register it
  if (idx == -1) {
    idx = registerCard(uid, size);
    if (idx == -1) {
      Serial.println("!! Card registry full. Cannot register new card.");
      beepRejected();
      return;
    }
    Serial.print(">> New card registered in slot ");
    Serial.println(idx);
  }

  CardRecord &card = cards[idx];

  // ---- TIME IN ----
  if (!card.isCheckedIn) {
    card.isCheckedIn = true;
    card.timeInMs    = millis();

    Serial.println(">>> TIME IN");
    Serial.print("    UID      : "); printUID(uid, size); Serial.println();
    Serial.print("    Time In  : "); Serial.println(formatTimestamp(card.timeInMs));
    beepSuccess();
  }

  // ---- TIME OUT (cooldown check) ----
  else {
    unsigned long elapsed = millis() - card.timeInMs;

    if (elapsed < MIN_CHECKOUT) {
      unsigned long remaining = (MIN_CHECKOUT - elapsed) / 1000;
      Serial.println(">>> TOO SOON — Cannot check out yet.");
      Serial.print("    Wait another ~"); Serial.print(remaining); Serial.println("s");
      beepRejected();
      return;
    }

    unsigned long timeOutMs = millis();
    card.isCheckedIn = false;

    Serial.println(">>> TIME OUT");
    Serial.print("    UID        : "); printUID(uid, size); Serial.println();
    Serial.print("    Time In    : "); Serial.println(formatTimestamp(card.timeInMs));
    Serial.print("    Time Out   : "); Serial.println(formatTimestamp(timeOutMs));
    Serial.print("    Duration   : "); Serial.println(formatDuration(elapsed));
    beepCheckout();
  }
}

// ===================== SETUP =====================
void setup() {
  Serial.begin(115200);
  delay(1000);

  SPI.begin(SCK_PIN, MISO_PIN, MOSI_PIN, SS_PIN);
  rfid.PCD_Init();

  pinMode(BUZZER_PIN, OUTPUT);
  digitalWrite(BUZZER_PIN, LOW);

  memset(cards, 0, sizeof(cards));

  Serial.println("========================================");
  Serial.println("   RFID Attendance System Ready");
  Serial.print  ("   Min checkout time: ");
  Serial.println(formatDuration(MIN_CHECKOUT));
  Serial.println("========================================");

  beep(1000);
}

// ===================== LOOP =====================
void loop() {
  if (millis() - lastScanTime < SCAN_COOLDOWN) return;

  if (!rfid.PICC_IsNewCardPresent()) return;
  if (!rfid.PICC_ReadCardSerial())   return;

  lastScanTime = millis();

  Serial.println("----------------------------------------");
  handleCard(rfid.uid.uidByte, rfid.uid.size);
  Serial.println("----------------------------------------");

  rfid.PICC_HaltA();
  rfid.PCD_StopCrypto1();
}