#ifndef MFRC522_I2C_EXTENDED_H
#define MFRC522_I2C_EXTENDED_H

#include "MFRC522_I2C.h"

class MFRC522_I2C_Extended : public MFRC522_I2C {
public:
  enum TagBitRates : byte {
    BITRATE_106KBITS = 0x00,
    BITRATE_212KBITS = 0x01,
    BITRATE_424KBITS = 0x02,
    BITRATE_848KBITS = 0x03,
  };

  struct Ats {
    byte size = 0;
    byte fsc = 32;
    struct {
      bool transmitted = false;
      bool sameD = false;
      TagBitRates ds = BITRATE_106KBITS;
      TagBitRates dr = BITRATE_106KBITS;
    } ta1;
    struct {
      bool transmitted = false;
      byte fwi = 0;
      byte sfgi = 0;
    } tb1;
    struct {
      bool transmitted = false;
      bool supportsCID = true;
      bool supportsNAD = false;
    } tc1;
    byte data[FIFO_SIZE - 2] = {0};
  };

  struct TagInfo {
    uint16_t atqa = 0;
    Uid uid = {};
    Ats ats = {};
    bool blockNumber = false;
  };

  struct PcbBlock {
    struct {
      byte pcb = 0;
      byte cid = 0;
      byte nad = 0;
    } prologue;
    struct {
      byte size = 0;
      const byte *data = nullptr;
    } inf;
  };

  explicit MFRC522_I2C_Extended(byte chipAddress = kDefaultAddress,
                                TwoWire *wire = &Wire)
      : MFRC522_I2C(chipAddress, wire) {}

  StatusCode PICC_Select(Uid *uid, byte validBits = 0) override;
  StatusCode PICC_RequestATS(Ats *ats);
  StatusCode PICC_PPS();
  StatusCode PICC_PPS(TagBitRates sendBitRate, TagBitRates receiveBitRate);

  StatusCode TCL_Transceive(const PcbBlock *send, byte *backData,
                            byte *backLen, byte *backPcb = nullptr);
  StatusCode TCL_Transceive(TagInfo *tag, const byte *sendData, byte sendLen,
                            byte *backData = nullptr, byte *backLen = nullptr);
  StatusCode TCL_TransceiveRBlock(TagInfo *tag, bool ack,
                                  byte *backData = nullptr,
                                  byte *backLen = nullptr);
  StatusCode TCL_Deselect(TagInfo *tag);

  bool PICC_IsNewCardPresent() override;
  bool PICC_ReadCardSerial() override;
  void clearTag();

  TagInfo tag;
};

#endif
