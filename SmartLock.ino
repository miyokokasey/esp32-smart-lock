#include <SPI.h>
#include <MFRC522.h>
#include <WiFi.h>
#include <WebServer.h>
#include "BluetoothSerial.h"

#define SS_PIN    5
#define RST_PIN   22
#define RELAY_PIN 32
#define GREEN_LED 25
#define RED_LED   26
#define BUZZER    27

// *** REPLACE THESE WITH YOUR WIFI DETAILS ***
const char* WIFI_SSID = "YourWiFiName";
const char* WIFI_PASS = "YourWiFiPassword";

// *** UPDATED WITH YOUR RECORDED CARD UID ***
byte authorizedUIDs[][4] = {
  {0x75, 0x5C, 0x44, 0xE0}, // Primary Card
  {0x8A, 0x51, 0xD4, 0x35}  // Secondary Tag (Optional)
};
const int NUM_UIDS = 2;

MFRC522 rfid(SS_PIN, RST_PIN);
BluetoothSerial SerialBT;
WebServer server(80);
bool isUnlocked = false;

void lockDoor() {
  isUnlocked = false;
  digitalWrite(RELAY_PIN, LOW); // Rest state: Relay OFF / Locked
  digitalWrite(GREEN_LED, LOW);
  Serial.println("LOCKED");
}

void unlockDoor() {
  if (isUnlocked) return;
  isUnlocked = true;
  digitalWrite(RELAY_PIN, HIGH); // Active state: Relay ON / Unlocked
  digitalWrite(GREEN_LED, HIGH);
  digitalWrite(RED_LED, LOW);
  tone(BUZZER, 1000, 200);
  Serial.println("UNLOCKED");
  delay(3000);
  lockDoor();
}

void denyAccess() {
  digitalWrite(RED_LED, HIGH);
  tone(BUZZER, 400, 400);
  Serial.println("DENIED");
  delay(1000);
  digitalWrite(RED_LED, LOW);
}

bool isAuthorized() {
  for (int i = 0; i < NUM_UIDS; i++) {
    bool match = true;
    for (byte j = 0; j < 4; j++) {
      if (rfid.uid.uidByte[j] != authorizedUIDs[i][j]) {
        match = false; 
        break;
      }
    }
    if (match) return true;
  }
  return false;
}

// Fixed HTML rendering using C++ Raw String Literals
void handleRoot() {
  String html = R"HTML(
    <!DOCTYPE html>
    <html>
    <head>
      <meta name='viewport' content='width=device-width, initial-scale=1'>
      <title>Smart Lock</title>
    </head>
    <body style='font-family:sans-serif; text-align:center; padding:40px;'>
      <h2>Smart Lock</h2>
      <a href='/unlock'>
        <button style='font-size:20px; padding:16px 32px; background:#1a73e8; color:white; border:none; border-radius:8px; cursor:pointer;'>
          Unlock Door
        </button>
      </a>
    </body>
    </html>
  )HTML";

  server.send(200, "text/html", html);
}

void handleUnlock() {
  String html = R"HTML(
    <!DOCTYPE html>
    <html>
    <body style='font-family:sans-serif; text-align:center; padding:40px;'>
      <h2 style='color:#188038;'>Unlocked</h2>
      <p>Re-locks in 3 seconds.</p>
      <a href='/'>Back</a>
    </body>
    </html>
  )HTML";

  server.send(200, "text/html", html);
  Serial.println("WiFi unlock");
  unlockDoor();
}

void setup() {
  Serial.begin(115200);
  SPI.begin();
  rfid.PCD_Init();
  SerialBT.begin("SmartLock");

  pinMode(RELAY_PIN, OUTPUT);
  pinMode(GREEN_LED, OUTPUT);
  pinMode(RED_LED, OUTPUT);
  pinMode(BUZZER, OUTPUT);
  
  digitalWrite(RELAY_PIN, LOW); // Initialized to locked state
  digitalWrite(GREEN_LED, LOW);
  digitalWrite(RED_LED, LOW);

  WiFi.begin(WIFI_SSID, WIFI_PASS);
  Serial.print("Connecting to WiFi");
  int tries = 0;
  while (WiFi.status() != WL_CONNECTED && tries < 20) {
    delay(500); 
    Serial.print("."); 
    tries++;
  }
  
  if (WiFi.status() == WL_CONNECTED) {
    Serial.println("\nIP: " + WiFi.localIP().toString());
    server.on("/", handleRoot);
    server.on("/unlock", handleUnlock);
    server.begin();
  } else {
    Serial.println("\nWiFi failed - RFID+BT still work");
  }
  
  Serial.println("System ready");
}

void loop() {
  server.handleClient();
  
  if (SerialBT.available()) {
    String cmd = SerialBT.readStringUntil('\n');
    cmd.trim();
    if (cmd == "UNLOCK") { 
      Serial.println("BT unlock"); 
      unlockDoor(); 
    }
  }
  
  if (!rfid.PICC_IsNewCardPresent()) return;
  if (!rfid.PICC_ReadCardSerial()) return;
  
  if (isAuthorized()) { 
    unlockDoor(); 
  } else { 
    denyAccess(); 
  }
  
  rfid.PICC_HaltA();
  rfid.PCD_StopCrypto1();
}