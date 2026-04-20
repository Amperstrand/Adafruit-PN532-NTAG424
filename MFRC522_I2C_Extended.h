#ifndef MFRC522_I2C_EXTENDED_H
#define MFRC522_I2C_EXTENDED_H

#include "MFRC522_I2C.h"

// ISO 14443-4 PCB bit field definitions.
// PCB byte layout: [b7 b6 b5 b4 b3 b2 b1 b0]
//   b7-b6: Block type (00=I, 10=R, 11=S)
//   b5-b4: Subtype (varies by block type)
//   b3:    CID following
//   b2:    NAD following
//   b1:    Always 1 (reserved)
//   b0:    Block number (0 or 1)
constexpr byte PCB_BLOCK_NUMBER   = 0x01;  // b0
constexpr byte PCB_RESERVED_BIT   = 0x02;  // b1 (always set)
constexpr byte PCB_NAD_FOLLOWING  = 0x04;  // b2
constexpr byte PCB_CID_FOLLOWING  = 0x08;  // b3
constexpr byte PCB_CHAINING       = 0x10;  // b4 (I-block chaining)
constexpr byte PCB_R_NAK          = 0x10;  // b4 (R-block: 0=ACK, 1=NAK)
constexpr byte PCB_R_FIXED        = 0x20;  // b5 (always 1 for R-block)
constexpr byte PCB_S_WTX          = 0x30;  // b4-b5 (S-block WTX subtype)
constexpr byte PCB_TYPE_MASK      = 0xC0;  // b7-b6
constexpr byte PCB_TYPE_I         = 0x00;  // I-block
constexpr byte PCB_TYPE_R         = 0x80;  // R-block
constexpr byte PCB_TYPE_S         = 0xC0;  // S-block

// Computed PCB base values
constexpr byte PCB_IBASE   = PCB_TYPE_I | PCB_RESERVED_BIT;                    // 0x02
constexpr byte PCB_RACK    = PCB_TYPE_R | PCB_R_FIXED | PCB_RESERVED_BIT;      // 0xA2
constexpr byte PCB_RNAK    = PCB_TYPE_R | PCB_R_FIXED | PCB_R_NAK | PCB_RESERVED_BIT; // 0xB2
constexpr byte PCB_SDESELECT = PCB_TYPE_S | PCB_RESERVED_BIT;                  // 0xC2
constexpr byte PCB_SWTX_RSP = PCB_TYPE_S | PCB_S_WTX | PCB_RESERVED_BIT;      // 0xF2

// MFRC522 register bit
constexpr byte MFRC522_CRC_ENABLE = 0x80;

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
