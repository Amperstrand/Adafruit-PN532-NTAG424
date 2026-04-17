#include "MFRC522_I2C.h"

#if defined(ARDUINO)
#include <Wire.h>
#endif

MFRC522_I2C::MFRC522_I2C(byte chipAddress, TwoWire *wire)
    : _chipAddress(chipAddress), _wire(wire != nullptr ? wire : &Wire) {}

bool MFRC522_I2C::PCD_WriteRegisterChecked(byte reg, byte value) {
  if (_wire == nullptr) {
    return false;
  }

  _wire->beginTransmission(_chipAddress);
  if (_wire->write(reg) != 1 || _wire->write(value) != 1) {
    return false;
  }
  return _wire->endTransmission() == 0;
}

bool MFRC522_I2C::PCD_ReadRegisterChecked(byte reg, byte *value) {
  if (_wire == nullptr || value == nullptr) {
    return false;
  }

  *value = 0;
  _wire->beginTransmission(_chipAddress);
  if (_wire->write(reg) != 1) {
    return false;
  }
  if (_wire->endTransmission() != 0) {
    return false;
  }
  if (_wire->requestFrom(_chipAddress, static_cast<size_t>(1)) != 1) {
    return false;
  }
  if (!_wire->available()) {
    return false;
  }

  *value = static_cast<byte>(_wire->read());
  return true;
}

void MFRC522_I2C::PCD_WriteRegister(byte reg, byte value) {
  if (_wire == nullptr) {
    return;
  }

  _wire->beginTransmission(_chipAddress);
  _wire->write(reg);
  _wire->write(value);
  _wire->endTransmission();
}

void MFRC522_I2C::PCD_WriteRegister(byte reg, byte count, const byte *values) {
  if (_wire == nullptr || values == nullptr || count == 0) {
    return;
  }

  byte offset = 0;
  while (offset < count) {
    const byte chunk =
        static_cast<byte>((count - offset) > kWireChunkSize
                              ? kWireChunkSize
                              : (count - offset));
    _wire->beginTransmission(_chipAddress);
    _wire->write(reg);
    _wire->write(values + offset, chunk);
    _wire->endTransmission();
    offset = static_cast<byte>(offset + chunk);
  }
}

byte MFRC522_I2C::PCD_ReadRegister(byte reg) {
  byte value = 0;
  PCD_ReadRegister(reg, 1, &value);
  return value;
}

void MFRC522_I2C::PCD_ReadRegister(byte reg, byte count, byte *values,
                                   byte rxAlign) {
  if (_wire == nullptr || values == nullptr || count == 0) {
    return;
  }

  memset(values, 0, count);
  byte offset = 0;
  while (offset < count) {
    const byte chunk =
        static_cast<byte>((count - offset) > kWireChunkSize
                              ? kWireChunkSize
                              : (count - offset));
    _wire->beginTransmission(_chipAddress);
    _wire->write(reg);
    _wire->endTransmission();
    _wire->requestFrom(_chipAddress, static_cast<size_t>(chunk));

    byte index = 0;
    while (_wire->available() && index < chunk) {
      const byte value = static_cast<byte>(_wire->read());
      if (offset == 0 && index == 0 && rxAlign != 0) {
        byte mask = 0;
        for (byte bit = rxAlign; bit <= 7; ++bit) {
          mask |= static_cast<byte>(1U << bit);
        }
        values[0] = static_cast<byte>((values[0] & ~mask) | (value & mask));
      } else {
        values[offset + index] = value;
      }
      ++index;
    }

    offset = static_cast<byte>(offset + chunk);
  }
}

void MFRC522_I2C::PCD_SetRegisterBitMask(byte reg, byte mask) {
  PCD_WriteRegister(reg, static_cast<byte>(PCD_ReadRegister(reg) | mask));
}

void MFRC522_I2C::PCD_ClearRegisterBitMask(byte reg, byte mask) {
  PCD_WriteRegister(reg, static_cast<byte>(PCD_ReadRegister(reg) & ~mask));
}

MFRC522_I2C::StatusCode MFRC522_I2C::PCD_CalculateCRC(const byte *data,
                                                      byte length,
                                                      byte *result) {
  if (data == nullptr || result == nullptr) {
    return STATUS_INVALID;
  }

  PCD_WriteRegister(CommandReg, PCD_Idle);
  PCD_WriteRegister(DivIrqReg, 0x04);
  PCD_SetRegisterBitMask(FIFOLevelReg, 0x80);
  PCD_WriteRegister(FIFODataReg, length, data);
  PCD_WriteRegister(CommandReg, PCD_CalcCRC);

  unsigned int retries = 5000;
  while (retries-- > 0) {
    const byte irq = PCD_ReadRegister(DivIrqReg);
    if ((irq & 0x04) != 0) {
      PCD_WriteRegister(CommandReg, PCD_Idle);
      result[0] = PCD_ReadRegister(CRCResultRegL);
      result[1] = PCD_ReadRegister(CRCResultRegH);
      return STATUS_OK;
    }
  }

  return STATUS_TIMEOUT;
}

bool MFRC522_I2C::PCD_Init() {
  if (!PCD_Reset()) {
    return false;
  }
  return PCD_ConfigureDefaults();
}

bool MFRC522_I2C::PCD_ConfigureDefaults() {
  if (!PCD_WriteRegisterChecked(TModeReg, 0x80) ||
      !PCD_WriteRegisterChecked(TPrescalerReg, 0xA9) ||
      !PCD_WriteRegisterChecked(TReloadRegH, 0x03) ||
      !PCD_WriteRegisterChecked(TReloadRegL, 0xE8) ||
      !PCD_WriteRegisterChecked(TxASKReg, 0x40) ||
      !PCD_WriteRegisterChecked(ModeReg, 0x3D)) {
    return false;
  }
  PCD_AntennaOn();
  clearTag();
  return true;
}

bool MFRC522_I2C::PCD_Reset() {
  if (!PCD_WriteRegisterChecked(CommandReg, PCD_SoftReset)) {
    return false;
  }
  delay(50);
  for (uint8_t retries = 0; retries < 100; ++retries) {
    byte command = 0;
    if (PCD_ReadRegisterChecked(CommandReg, &command) &&
        (command & (1U << 4)) == 0) {
      return true;
    }
    delay(1);
  }
  return false;
}

void MFRC522_I2C::PCD_AntennaOn() {
  const byte value = PCD_ReadRegister(TxControlReg);
  if ((value & 0x03) != 0x03) {
    PCD_WriteRegister(TxControlReg, static_cast<byte>(value | 0x03));
  }
}

void MFRC522_I2C::PCD_AntennaOff() {
  PCD_ClearRegisterBitMask(TxControlReg, 0x03);
}

MFRC522_I2C::StatusCode MFRC522_I2C::PCD_TransceiveData(
    const byte *sendData, byte sendLen, byte *backData, byte *backLen,
    byte *validBits, byte rxAlign, bool checkCRC) {
  return PCD_CommunicateWithPICC(PCD_Transceive, 0x30, sendData, sendLen,
                                 backData, backLen, validBits, rxAlign,
                                 checkCRC);
}

MFRC522_I2C::StatusCode MFRC522_I2C::PCD_CommunicateWithPICC(
    byte command, byte waitIRq, const byte *sendData, byte sendLen,
    byte *backData, byte *backLen, byte *validBits, byte rxAlign,
    bool checkCRC) {
  if (sendData == nullptr || sendLen == 0) {
    return STATUS_INVALID;
  }

  const byte txLastBits = validBits != nullptr ? *validBits : 0;
  const byte bitFraming = static_cast<byte>((rxAlign << 4) | txLastBits);

  PCD_WriteRegister(CommandReg, PCD_Idle);
  PCD_WriteRegister(ComIrqReg, 0x7F);
  PCD_SetRegisterBitMask(FIFOLevelReg, 0x80);
  PCD_WriteRegister(FIFODataReg, sendLen, sendData);
  PCD_WriteRegister(BitFramingReg, bitFraming);
  PCD_WriteRegister(CommandReg, command);
  if (command == PCD_Transceive) {
    PCD_SetRegisterBitMask(BitFramingReg, 0x80);
  }

  // MFRC522 exposes a programmable timer unit rather than a card-aware ISO-DEP
  // timeout policy (MFRC522 datasheet Rev. 3.9, Section 8.5 "Timer unit").
  // NTAG X DNA documentation shows that Type 4 Tag operations may need longer
  // host-side wait windows while the PICC finishes work and/or emits WTX
  // requests (AN14513 Rev. 3.0, Section 4.2; AN12196 Rev. 2.0, Section 5.8.1).
  // Give longer write-like transceive operations more poll budget instead of
  // timing out at the same short loop used for select/read traffic.
  unsigned int retries =
      (command == PCD_Transceive && sendLen > 16) ? kWritePcdPollRetries
                                                 : kDefaultPcdPollRetries;
  while (retries-- > 0) {
    const byte irq = PCD_ReadRegister(ComIrqReg);
    if ((irq & waitIRq) != 0) {
      break;
    }
    if ((irq & 0x01) != 0) {
      return STATUS_TIMEOUT;
    }
  }

  if (retries == 0) {
    return STATUS_TIMEOUT;
  }

  const byte error = PCD_ReadRegister(ErrorReg);
  if ((error & 0x13) != 0) {
    return STATUS_ERROR;
  }

  byte receivedBits = 0;
  if (backData != nullptr && backLen != nullptr) {
    const byte size = PCD_ReadRegister(FIFOLevelReg);
    if (size > *backLen) {
      return STATUS_NO_ROOM;
    }

    *backLen = size;
    PCD_ReadRegister(FIFODataReg, size, backData, rxAlign);
    receivedBits = static_cast<byte>(PCD_ReadRegister(ControlReg) & 0x07);
    if (validBits != nullptr) {
      *validBits = receivedBits;
    }
  }

  if ((error & 0x08) != 0) {
    return STATUS_COLLISION;
  }

  if (backData != nullptr && backLen != nullptr && checkCRC) {
    if (*backLen == 1 && receivedBits == 4) {
      return STATUS_MIFARE_NACK;
    }
    if (*backLen < 2 || receivedBits != 0) {
      return STATUS_CRC_WRONG;
    }

    byte controlBuffer[2] = {0};
    const StatusCode status =
        PCD_CalculateCRC(backData, static_cast<byte>(*backLen - 2), controlBuffer);
    if (status != STATUS_OK) {
      return status;
    }
    if (backData[*backLen - 2] != controlBuffer[0] ||
        backData[*backLen - 1] != controlBuffer[1]) {
      return STATUS_CRC_WRONG;
    }
  }

  return STATUS_OK;
}

MFRC522_I2C::StatusCode MFRC522_I2C::PICC_RequestA(byte *bufferATQA,
                                                   byte *bufferSize) {
  return PICC_REQA_or_WUPA(PICC_CMD_REQA, bufferATQA, bufferSize);
}

MFRC522_I2C::StatusCode MFRC522_I2C::PICC_WakeupA(byte *bufferATQA,
                                                  byte *bufferSize) {
  return PICC_REQA_or_WUPA(PICC_CMD_WUPA, bufferATQA, bufferSize);
}

MFRC522_I2C::StatusCode MFRC522_I2C::PICC_REQA_or_WUPA(byte command,
                                                       byte *bufferATQA,
                                                       byte *bufferSize) {
  if (bufferATQA == nullptr || bufferSize == nullptr || *bufferSize < 2) {
    return STATUS_NO_ROOM;
  }

  PCD_ClearRegisterBitMask(CollReg, 0x80);
  byte validBits = 7;
  const StatusCode status =
      PCD_TransceiveData(&command, 1, bufferATQA, bufferSize, &validBits);
  if (status != STATUS_OK) {
    return status;
  }
  if (*bufferSize != 2 || validBits != 0) {
    return STATUS_ERROR;
  }
  return STATUS_OK;
}

MFRC522_I2C::StatusCode MFRC522_I2C::PICC_Select(Uid *selectedUid,
                                                 byte validBits) {
  if (selectedUid == nullptr || validBits > 80) {
    return STATUS_INVALID;
  }

  bool uidComplete = false;
  byte cascadeLevel = 1;
  PCD_ClearRegisterBitMask(CollReg, 0x80);

  while (!uidComplete) {
    byte buffer[9] = {0};
    byte uidIndex = 0;
    bool useCascadeTag = false;

    switch (cascadeLevel) {
    case 1:
      buffer[0] = PICC_CMD_SEL_CL1;
      uidIndex = 0;
      useCascadeTag = validBits != 0 && selectedUid->size > 4;
      break;
    case 2:
      buffer[0] = PICC_CMD_SEL_CL2;
      uidIndex = 3;
      useCascadeTag = validBits != 0 && selectedUid->size > 7;
      break;
    case 3:
      buffer[0] = PICC_CMD_SEL_CL3;
      uidIndex = 6;
      break;
    default:
      return STATUS_INTERNAL_ERROR;
    }

    int8_t currentLevelKnownBits = static_cast<int8_t>(validBits - (8 * uidIndex));
    if (currentLevelKnownBits < 0) {
      currentLevelKnownBits = 0;
    }

    byte index = 2;
    if (useCascadeTag) {
      buffer[index++] = PICC_CMD_CT;
    }

    byte bytesToCopy = static_cast<byte>(currentLevelKnownBits / 8 +
                                         ((currentLevelKnownBits % 8) != 0 ? 1 : 0));
    if (bytesToCopy != 0) {
      const byte maxBytes = useCascadeTag ? 3 : 4;
      if (bytesToCopy > maxBytes) {
        bytesToCopy = maxBytes;
      }
      for (byte count = 0; count < bytesToCopy; ++count) {
        buffer[index++] = selectedUid->uidByte[uidIndex + count];
      }
    }
    if (useCascadeTag) {
      currentLevelKnownBits += 8;
    }

    bool selectDone = false;
    while (!selectDone) {
      byte txLastBits = 0;
      byte bufferUsed = 0;
      byte *responseBuffer = nullptr;
      byte responseLength = 0;

      if (currentLevelKnownBits >= 32) {
        buffer[1] = 0x70;
        buffer[6] = static_cast<byte>(buffer[2] ^ buffer[3] ^ buffer[4] ^
                                      buffer[5]);
        const StatusCode crcStatus = PCD_CalculateCRC(buffer, 7, &buffer[7]);
        if (crcStatus != STATUS_OK) {
          return crcStatus;
        }
        bufferUsed = 9;
        responseBuffer = &buffer[6];
        responseLength = 3;
      } else {
        txLastBits = static_cast<byte>(currentLevelKnownBits % 8);
        const byte count = static_cast<byte>(currentLevelKnownBits / 8);
        index = static_cast<byte>(2 + count);
        buffer[1] = static_cast<byte>((index << 4) | txLastBits);
        bufferUsed = static_cast<byte>(index + (txLastBits != 0 ? 1 : 0));
        responseBuffer = &buffer[index];
        responseLength = static_cast<byte>(sizeof(buffer) - index);
      }

      const byte rxAlign = txLastBits;
      PCD_WriteRegister(BitFramingReg,
                        static_cast<byte>((rxAlign << 4) | txLastBits));

      StatusCode result = PCD_TransceiveData(buffer, bufferUsed, responseBuffer,
                                             &responseLength, &txLastBits,
                                             rxAlign);
      if (result == STATUS_COLLISION) {
        const byte collReg = PCD_ReadRegister(CollReg);
        if ((collReg & 0x20) != 0) {
          return STATUS_COLLISION;
        }

        byte collisionPos = static_cast<byte>(collReg & 0x1F);
        if (collisionPos == 0) {
          collisionPos = 32;
        }
        if (collisionPos <= currentLevelKnownBits) {
          return STATUS_INTERNAL_ERROR;
        }

        currentLevelKnownBits = collisionPos;
        const byte bit = static_cast<byte>((currentLevelKnownBits - 1) % 8);
        index = static_cast<byte>(1 + (currentLevelKnownBits / 8) +
                                  (bit != 0 ? 1 : 0));
        buffer[index] |= static_cast<byte>(1U << bit);
        continue;
      }
      if (result != STATUS_OK) {
        return result;
      }

      if (currentLevelKnownBits >= 32) {
        selectDone = true;
      } else {
        currentLevelKnownBits = 32;
      }
    }

    index = buffer[2] == PICC_CMD_CT ? 3 : 2;
    bytesToCopy = buffer[2] == PICC_CMD_CT ? 3 : 4;
    for (byte count = 0; count < bytesToCopy; ++count) {
      selectedUid->uidByte[uidIndex + count] = buffer[index++];
    }

    if (buffer[6 + 1] == 0 && buffer[6 + 2] == 0) {
      return STATUS_ERROR;
    }

    byte crcBuffer[2] = {0};
    const StatusCode crcStatus = PCD_CalculateCRC(&buffer[6], 1, crcBuffer);
    if (crcStatus != STATUS_OK) {
      return crcStatus;
    }
    if (crcBuffer[0] != buffer[7] || crcBuffer[1] != buffer[8]) {
      return STATUS_CRC_WRONG;
    }

    if ((buffer[6] & 0x04) != 0) {
      ++cascadeLevel;
    } else {
      uidComplete = true;
      selectedUid->sak = buffer[6];
    }
  }

  selectedUid->size = static_cast<byte>(3 * cascadeLevel + 1);
  uid = *selectedUid;
  _tag_selected = true;
  return STATUS_OK;
}

MFRC522_I2C::StatusCode MFRC522_I2C::PICC_HaltA() {
  byte buffer[4] = {PICC_CMD_HLTA, 0x00, 0x00, 0x00};
  const StatusCode crcStatus = PCD_CalculateCRC(buffer, 2, &buffer[2]);
  if (crcStatus != STATUS_OK) {
    return crcStatus;
  }

  byte validBits = 0;
  byte responseLength = 0;
  const StatusCode status = PCD_TransceiveData(buffer, sizeof(buffer), nullptr,
                                               &responseLength, &validBits);
  clearTag();
  if (status == STATUS_TIMEOUT) {
    return STATUS_OK;
  }
  if (status == STATUS_OK) {
    return STATUS_ERROR;
  }
  return status;
}

bool MFRC522_I2C::PICC_IsNewCardPresent() {
  byte bufferATQA[2] = {0};
  byte bufferSize = sizeof(bufferATQA);
  const StatusCode status = PICC_RequestA(bufferATQA, &bufferSize);
  if (status == STATUS_OK || status == STATUS_COLLISION) {
    clearTag();
    return true;
  }
  return false;
}

bool MFRC522_I2C::PICC_ReadCardSerial() {
  return PICC_Select(&uid) == STATUS_OK;
}

MFRC522_I2C::PICC_Type MFRC522_I2C::PICC_GetType(byte sak) const {
  const byte normalizedSak = static_cast<byte>(sak & 0x7F);
  switch (normalizedSak) {
  case 0x04:
    return PICC_TYPE_NOT_COMPLETE;
  case 0x09:
    return PICC_TYPE_MIFARE_MINI;
  case 0x08:
    return PICC_TYPE_MIFARE_1K;
  case 0x18:
    return PICC_TYPE_MIFARE_4K;
  case 0x00:
    return PICC_TYPE_MIFARE_UL;
  case 0x10:
  case 0x11:
    return PICC_TYPE_MIFARE_PLUS;
  case 0x01:
    return PICC_TYPE_TNP3XXX;
  case 0x20:
    return PICC_TYPE_ISO_14443_4;
  case 0x40:
    return PICC_TYPE_ISO_18092;
  default:
    return PICC_TYPE_UNKNOWN;
  }
}

void MFRC522_I2C::getUID(uint8_t *uidBuffer, uint8_t *uidLength) const {
  if (uidLength == nullptr) {
    return;
  }

  *uidLength = uid.size;
  if (uidBuffer != nullptr && uid.size > 0) {
    memcpy(uidBuffer, uid.uidByte, uid.size);
  }
}

void MFRC522_I2C::clearTag() {
  memset(&uid, 0, sizeof(uid));
  _tag_selected = false;
}
