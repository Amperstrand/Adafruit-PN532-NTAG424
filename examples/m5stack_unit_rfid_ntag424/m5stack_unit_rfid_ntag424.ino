#include <MFRC522_NTAG424.h>
#include <Wire.h>

static constexpr uint8_t M5STACK_UNIT_RFID_ADDRESS = 0x28;
static constexpr int M5STACK_GROVE_SDA = 26;
static constexpr int M5STACK_GROVE_SCL = 32;

MFRC522_NTAG424 nfc(M5STACK_UNIT_RFID_ADDRESS, &Wire);

void printBytes(const uint8_t *data, uint8_t length) {
  for (uint8_t i = 0; i < length; ++i) {
    if (data[i] < 0x10) {
      Serial.print('0');
    }
    Serial.print(data[i], HEX);
    if (i + 1 < length) {
      Serial.print(' ');
    }
  }
  Serial.println();
}

void setup() {
  Serial.begin(115200);
  delay(1000);

  if (!nfc.begin(M5STACK_GROVE_SDA, M5STACK_GROVE_SCL)) {
    Serial.println("MFRC522 I2C init failed");
    while (true) {
      delay(1000);
    }
  }

  Serial.println("Waiting for NTAG424 on M5Stack Unit RFID...");
}

void loop() {
  uint8_t uid[10] = {0};
  uint8_t uidLength = 0;
  if (!nfc.readPassiveTargetID(uid, &uidLength)) {
    delay(200);
    return;
  }

  Serial.print("UID: ");
  printBytes(uid, uidLength);

  if (!nfc.ntag424_isNTAG424()) {
    Serial.println("Detected ISO14443A tag is not NTAG424");
    delay(1000);
    return;
  }

  Serial.println("Detected NTAG424");

  uint8_t filename[7] = {0xD2, 0x76, 0x00, 0x00, 0x85, 0x01, 0x01};
  if (!nfc.ntag424_ISOSelectFileByDFN(filename)) {
    Serial.println("ISOSelectFileByDFN failed");
    delay(1000);
    return;
  }

  uint8_t key[16] = {0};
  if (nfc.ntag424_Authenticate(key, 0, 0x71) != 1) {
    Serial.println("Authentication failed");
    delay(1000);
    return;
  }

  uint8_t cardUid[16] = {0};
  const uint8_t cardUidLength = nfc.ntag424_GetCardUID(cardUid);
  Serial.print("Card UID: ");
  printBytes(cardUid, cardUidLength);

  uint8_t ttStatus[16] = {0};
  const uint8_t ttStatusLength = nfc.ntag424_GetTTStatus(ttStatus);
  Serial.print("TT Status: ");
  printBytes(ttStatus, ttStatusLength);

  delay(2000);
}
