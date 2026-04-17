#ifndef MFRC522_I2C_H
#define MFRC522_I2C_H

#if __has_include("Arduino.h")
#include "Arduino.h"
#else
#include <stddef.h>
#include <stdint.h>

typedef uint8_t byte;
class __FlashStringHelper;

#ifndef PROGMEM
#define PROGMEM
#endif
#ifndef F
#define F(x) (x)
#endif
#ifndef HEX
#define HEX 16
#endif

inline uint8_t pgm_read_byte(const uint8_t *pointer) { return *pointer; }
inline void delay(unsigned long) {}
inline void delayMicroseconds(unsigned int) {}
#endif

#include <string.h>

#if defined(ARDUINO)
class TwoWire;
extern TwoWire Wire;
#else
class TwoWire {
public:
  bool begin() { return true; }
  bool begin(int, int, uint32_t = 0) { return true; }
  void beginTransmission(uint8_t) {}
  size_t write(uint8_t) { return 1; }
  size_t write(const uint8_t *, size_t len) { return len; }
  int endTransmission(bool = true) { return 0; }
  size_t requestFrom(uint8_t, size_t quantity) {
    _available = quantity;
    return quantity;
  }
  int available() { return _available > 0 ? 1 : 0; }
  int read() {
    if (_available > 0) {
      --_available;
    }
    return 0;
  }

private:
  size_t _available = 0;
};

inline TwoWire Wire;
#endif

class MFRC522_I2C {
public:
  static constexpr byte kDefaultAddress = 0x28;
  static constexpr byte FIFO_SIZE = 64;

  enum PCD_Register : byte {
    CommandReg = 0x01,
    ComIEnReg = 0x02,
    DivIEnReg = 0x03,
    ComIrqReg = 0x04,
    DivIrqReg = 0x05,
    ErrorReg = 0x06,
    Status1Reg = 0x07,
    Status2Reg = 0x08,
    FIFODataReg = 0x09,
    FIFOLevelReg = 0x0A,
    WaterLevelReg = 0x0B,
    ControlReg = 0x0C,
    BitFramingReg = 0x0D,
    CollReg = 0x0E,
    ModeReg = 0x11,
    TxModeReg = 0x12,
    RxModeReg = 0x13,
    TxControlReg = 0x14,
    TxASKReg = 0x15,
    TxSelReg = 0x16,
    RxSelReg = 0x17,
    RxThresholdReg = 0x18,
    DemodReg = 0x19,
    MfTxReg = 0x1C,
    MfRxReg = 0x1D,
    SerialSpeedReg = 0x1F,
    CRCResultRegH = 0x21,
    CRCResultRegL = 0x22,
    ModWidthReg = 0x24,
    RFCfgReg = 0x26,
    GsNReg = 0x27,
    CWGsPReg = 0x28,
    ModGsPReg = 0x29,
    TModeReg = 0x2A,
    TPrescalerReg = 0x2B,
    TReloadRegH = 0x2C,
    TReloadRegL = 0x2D,
    TCounterValueRegH = 0x2E,
    TCounterValueRegL = 0x2F,
    TestSel1Reg = 0x31,
    TestSel2Reg = 0x32,
    TestPinEnReg = 0x33,
    TestPinValueReg = 0x34,
    TestBusReg = 0x35,
    AutoTestReg = 0x36,
    VersionReg = 0x37,
    AnalogTestReg = 0x38,
    TestDAC1Reg = 0x39,
    TestDAC2Reg = 0x3A,
    TestADCReg = 0x3B
  };

  enum PCD_Command : byte {
    PCD_Idle = 0x00,
    PCD_Mem = 0x01,
    PCD_GenerateRandomID = 0x02,
    PCD_CalcCRC = 0x03,
    PCD_Transmit = 0x04,
    PCD_NoCmdChange = 0x07,
    PCD_Receive = 0x08,
    PCD_Transceive = 0x0C,
    PCD_MFAuthent = 0x0E,
    PCD_SoftReset = 0x0F
  };

  enum PICC_Command : byte {
    PICC_CMD_REQA = 0x26,
    PICC_CMD_WUPA = 0x52,
    PICC_CMD_CT = 0x88,
    PICC_CMD_SEL_CL1 = 0x93,
    PICC_CMD_SEL_CL2 = 0x95,
    PICC_CMD_SEL_CL3 = 0x97,
    PICC_CMD_HLTA = 0x50,
    PICC_CMD_RATS = 0xE0
  };

  enum PICC_Type : byte {
    PICC_TYPE_UNKNOWN = 0,
    PICC_TYPE_ISO_14443_4 = 1,
    PICC_TYPE_ISO_18092 = 2,
    PICC_TYPE_MIFARE_MINI = 3,
    PICC_TYPE_MIFARE_1K = 4,
    PICC_TYPE_MIFARE_4K = 5,
    PICC_TYPE_MIFARE_UL = 6,
    PICC_TYPE_MIFARE_PLUS = 7,
    PICC_TYPE_TNP3XXX = 8,
    PICC_TYPE_NOT_COMPLETE = 255
  };

  enum StatusCode : byte {
    STATUS_OK = 1,
    STATUS_ERROR = 2,
    STATUS_COLLISION = 3,
    STATUS_TIMEOUT = 4,
    STATUS_NO_ROOM = 5,
    STATUS_INTERNAL_ERROR = 6,
    STATUS_INVALID = 7,
    STATUS_CRC_WRONG = 8,
    STATUS_MIFARE_NACK = 9
  };

  struct Uid {
    byte size = 0;
    byte uidByte[10] = {0};
    byte sak = 0;
  };

  explicit MFRC522_I2C(byte chipAddress = kDefaultAddress,
                       TwoWire *wire = &Wire);

  void PCD_WriteRegister(byte reg, byte value);
  void PCD_WriteRegister(byte reg, byte count, const byte *values);
  byte PCD_ReadRegister(byte reg);
  void PCD_ReadRegister(byte reg, byte count, byte *values, byte rxAlign = 0);
  bool PCD_WriteRegisterChecked(byte reg, byte value);
  bool PCD_ReadRegisterChecked(byte reg, byte *value);
  void PCD_SetRegisterBitMask(byte reg, byte mask);
  void PCD_ClearRegisterBitMask(byte reg, byte mask);
  StatusCode PCD_CalculateCRC(const byte *data, byte length, byte *result);

  bool PCD_Init();
  bool PCD_Reset();
  bool PCD_ConfigureDefaults();
  void PCD_AntennaOn();
  void PCD_AntennaOff();

  StatusCode PCD_TransceiveData(const byte *sendData, byte sendLen,
                                byte *backData, byte *backLen,
                                byte *validBits = nullptr, byte rxAlign = 0,
                                bool checkCRC = false);
  StatusCode PCD_CommunicateWithPICC(byte command, byte waitIRq,
                                     const byte *sendData, byte sendLen,
                                     byte *backData = nullptr,
                                     byte *backLen = nullptr,
                                     byte *validBits = nullptr,
                                     byte rxAlign = 0,
                                     bool checkCRC = false);

  StatusCode PICC_RequestA(byte *bufferATQA, byte *bufferSize);
  StatusCode PICC_WakeupA(byte *bufferATQA, byte *bufferSize);
  StatusCode PICC_REQA_or_WUPA(byte command, byte *bufferATQA,
                               byte *bufferSize);
  virtual StatusCode PICC_Select(Uid *uid, byte validBits = 0);
  StatusCode PICC_HaltA();

  virtual bool PICC_IsNewCardPresent();
  virtual bool PICC_ReadCardSerial();

  PICC_Type PICC_GetType(byte sak) const;
  void getUID(uint8_t *uidBuffer, uint8_t *uidLength) const;
  bool isTagPresent() const { return _tag_selected; }
  void clearTag();

  Uid uid;

protected:
  static constexpr byte kWireChunkSize = 24;
  static constexpr unsigned int kDefaultPcdPollRetries = 2000;
  static constexpr unsigned int kWritePcdPollRetries = 12000;

  byte _chipAddress;
  TwoWire *_wire;
  bool _tag_selected = false;
};

#endif
