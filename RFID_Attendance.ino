#include <SPI.h>
#include <MFRC522.h>
#include <BluetoothSerial.h>

// ===================== PIN CONFIG =====================
#define SS_PIN     21
#define RST_PIN    4
#define SCK_PIN    18
#define MISO_PIN   19
#define MOSI_PIN   23
#define BUZZER_PIN 25

MFRC522 rfid(SS_PIN, RST_PIN);
BluetoothSerial BT;

// ===================== SETTINGS =====================
const unsigned long SCAN_COOLDOWN = 2000;
const unsigned long MIN_CHECKOUT  = 10000;  // ⬅ Change this (ms)
const int MAX_CARDS = 5;

#define BT_DEVICE_NAME "RFID-Attendance"  // ⬅ Name that shows up when pairing

// ===================== CARD STATE =====================
struct CardRecord {
  byte uid[10];
  byte uidSize;
  bool isCheckedIn;
  unsigned long timeInMs;
  bool active;
};

CardRecord cards[MAX_CARDS];
unsigned long lastScanTime = 0;

// ===================== DUAL OUTPUT =====================
// Writes to both Serial and Bluetooth simultaneously
void dualPrint(String msg) {
  Serial.print(msg);
  if (BT.connected()) BT.print(msg);
}

void dualPrintln(String msg) {
  Serial.println(msg);
  if (BT.connected()) BT.println(msg);
}

void dualPrintln() {
  Serial.println();
  if (BT.connected()) BT.println();
}

// ===================== HELPERS =====================
bool uidMatch(byte *a, byte sizeA, byte *b, byte sizeB) {
  if (sizeA != sizeB) return false;
  for (byte i = 0; i < sizeA; i++) {
    if (a[i] != b[i]) return false;
  }
  return true;
}

void dualPrintUID(byte *uid, byte size) {
  String out = "";
  for (byte i = 0; i < size; i++) {
    if (uid[i] < 0x10) out += "0";
    out += String(uid[i], HEX);
    if (i < size - 1) out += ":";
  }
  out.toUpperCase();
  dualPrint(out);
}

String formatDuration(unsigned long ms) {
  unsigned long totalSec = ms / 1000;
  unsigned long hours   = totalSec / 3600;
  unsigned long minutes = (totalSec % 3600) / 60;
  unsigned long seconds = totalSec % 60;

  String result = "";
  if (hours > 0)   result += String(hours)   + "h ";
  if (minutes > 0) result += String(minutes) + "m ";
  result += String(seconds) + "s";
  return result;
}

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

void beepSuccess()  { beep(150); delay(80); beep(150); }  // Time In
void beepCheckout() { beep(600); }                         // Time Out
void beepRejected() { beep(80); delay(60); beep(80); delay(60); beep(80); } // Too soon
void beepBTConn()   { beep(100); delay(50); beep(100); delay(50); beep(300); } // BT connected

// ===================== CARD LOOKUP / REGISTER =====================
int findCard(byte *uid, byte size) {
  for (int i = 0; i < MAX_CARDS; i++) {
    if (cards[i].active && uidMatch(cards[i].uid, cards[i].uidSize, uid, size))
      return i;
  }
  return -1;
}

int registerCard(byte *uid, byte size) {
  for (int i = 0; i < MAX_CARDS; i++) {
    if (!cards[i].active) {
      memcpy(cards[i].uid, uid, size);
      cards[i].uidSize     = size;
      cards[i].isCheckedIn = false;
      cards[i].timeInMs    = 0;
      cards[i].active      = true;
      return i;
    }
  }
  return -1;
}

// ===================== BT COMMAND HANDLER =====================
// Allows the connected device to send simple commands over BT
void handleBTCommand(String cmd) {
  cmd.trim();
  cmd.toUpperCase();

  if (cmd == "STATUS") {
    dualPrintln("=== CURRENT STATUS ===");
    bool anyActive = false;
    for (int i = 0; i < MAX_CARDS; i++) {
      if (cards[i].active) {
        anyActive = true;
        dualPrint("Slot " + String(i) + " | UID: ");
        dualPrintUID(cards[i].uid, cards[i].uidSize);
        if (cards[i].isCheckedIn) {
          unsigned long elapsed = millis() - cards[i].timeInMs;
          dualPrintln(" | IN  | Duration so far: " + formatDuration(elapsed));
        } else {
          dualPrintln(" | OUT");
        }
      }
    }
    if (!anyActive) dualPrintln("No cards registered yet.");
    dualPrintln("======================");

  } else if (cmd == "HELP") {
    dualPrintln("=== BT COMMANDS ===");
    dualPrintln("STATUS  - Show all registered cards and their state");
    dualPrintln("CLEAR   - Clear all card records");
    dualPrintln("HELP    - Show this message");
    dualPrintln("===================");

  } else if (cmd == "CLEAR") {
    memset(cards, 0, sizeof(cards));
    dualPrintln(">> All card records cleared.");

  } else {
    dualPrintln("Unknown command: " + cmd + " (type HELP for commands)");
  }
}

// ===================== ATTENDANCE LOGIC =====================
void handleCard(byte *uid, byte size) {
  int idx = findCard(uid, size);

  if (idx == -1) {
    idx = registerCard(uid, size);
    if (idx == -1) {
      dualPrintln("!! Registry full. Cannot register new card.");
      beepRejected();
      return;
    }
    dualPrintln(">> New card registered in slot " + String(idx));
  }

  CardRecord &card = cards[idx];

  // ---- TIME IN ----
  if (!card.isCheckedIn) {
    card.isCheckedIn = true;
    card.timeInMs    = millis();

    dualPrintln(">>> TIME IN");
    dualPrint  ("    UID     : "); dualPrintUID(uid, size); dualPrintln();
    dualPrintln("    Time In : " + formatTimestamp(card.timeInMs));
    beepSuccess();
  }

  // ---- TIME OUT ----
  else {
    unsigned long elapsed = millis() - card.timeInMs;

    if (elapsed < MIN_CHECKOUT) {
      unsigned long remaining = (MIN_CHECKOUT - elapsed) / 1000;
      dualPrintln(">>> TOO SOON — Cannot check out yet.");
      dualPrintln("    Wait another ~" + String(remaining) + "s");
      beepRejected();
      return;
    }

    unsigned long timeOutMs  = millis();
    card.isCheckedIn = false;

    dualPrintln(">>> TIME OUT");
    dualPrint  ("    UID      : "); dualPrintUID(uid, size); dualPrintln();
    dualPrintln("    Time In  : " + formatTimestamp(card.timeInMs));
    dualPrintln("    Time Out : " + formatTimestamp(timeOutMs));
    dualPrintln("    Duration : " + formatDuration(elapsed));
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

  // Start Bluetooth
  BT.begin(BT_DEVICE_NAME);

  Serial.println("========================================");
  Serial.println("   RFID Attendance System Ready");
  Serial.println("   Bluetooth: " + String(BT_DEVICE_NAME));
  Serial.println("   Min checkout time: " + formatDuration(MIN_CHECKOUT));
  Serial.println("========================================");

  beep(1000);
}

// ===================== LOOP =====================
void loop() {

  // Handle incoming BT commands
  if (BT.connected()) {
    static String btBuffer = "";
    while (BT.available()) {
      char c = BT.read();
      if (c == '\n' || c == '\r') {
        if (btBuffer.length() > 0) {
          handleBTCommand(btBuffer);
          btBuffer = "";
        }
      } else {
        btBuffer += c;
      }
    }
  }

  // Anti-spam cooldown
  if (millis() - lastScanTime < SCAN_COOLDOWN) return;

  if (!rfid.PICC_IsNewCardPresent()) return;
  if (!rfid.PICC_ReadCardSerial())   return;

  lastScanTime = millis();

  dualPrintln("----------------------------------------");
  handleCard(rfid.uid.uidByte, rfid.uid.size);
  dualPrintln("----------------------------------------");

  rfid.PICC_HaltA();
  rfid.PCD_StopCrypto1();
}