#include "MFRC522_I2C_Extended.h"

namespace {

constexpr byte kMaxRetries = 2;

bool is_s_block_wtx(byte pcb, byte infLength, const byte *inf) {
  return (pcb & PCB_TYPE_MASK) == PCB_TYPE_S && (pcb & PCB_S_WTX) == PCB_S_WTX &&
         infLength == 1 && inf != nullptr;
}

}

MFRC522_I2C::StatusCode MFRC522_I2C_Extended::PICC_Select(Uid *selectedUid,
                                                          byte validBits) {
  if (selectedUid == nullptr) {
    return STATUS_INVALID;
  }

  const StatusCode status = MFRC522_I2C::PICC_Select(selectedUid, validBits);
  if (status != STATUS_OK) {
    return status;
  }

  tag.uid = *selectedUid;
  tag.isoDepState = IsoDepState::SELECTED;
  if ((selectedUid->sak & 0x24) == 0x20) {
    const StatusCode atsStatus = PICC_RequestATS(&tag.ats);
    if (atsStatus != STATUS_OK) {
      return atsStatus;
    }

    const StatusCode ppsStatus = PICC_PPS();
    if (ppsStatus != STATUS_OK) {
      PCD_WriteRegister(
          TxModeReg,
          static_cast<byte>(PCD_ReadRegister(TxModeReg) | MFRC522_CRC_ENABLE));
      PCD_WriteRegister(
          RxModeReg,
          static_cast<byte>(PCD_ReadRegister(RxModeReg) | MFRC522_CRC_ENABLE));
    }
  }

  return STATUS_OK;
}

MFRC522_I2C::StatusCode MFRC522_I2C_Extended::PICC_RequestATS(Ats *ats) {
  if (ats == nullptr) {
    return STATUS_INVALID;
  }

  byte buffer[FIFO_SIZE] = {0};
  byte bufferSize = FIFO_SIZE;
  buffer[0] = PICC_CMD_RATS;
  buffer[1] = 0x50;

  StatusCode status = PCD_CalculateCRC(buffer, 2, &buffer[2]);
  if (status != STATUS_OK) {
    return status;
  }

  status = PCD_TransceiveData(buffer, 4, buffer, &bufferSize, nullptr, 0, true);
  if (status != STATUS_OK) {
    return status;
  }

  memset(ats, 0, sizeof(*ats));
  ats->fsc = 32;
  ats->tc1.supportsCID = true;
  ats->size = buffer[0];

  if (bufferSize >= 2) {
    switch (buffer[1] & 0x0F) {
    case 0x00:
      ats->fsc = 16;
      break;
    case 0x01:
      ats->fsc = 24;
      break;
    case 0x02:
      ats->fsc = 32;
      break;
    case 0x03:
      ats->fsc = 40;
      break;
    case 0x04:
      ats->fsc = 48;
      break;
    case 0x05:
      ats->fsc = 64;
      break;
    case 0x06:
      ats->fsc = 96;
      break;
    case 0x07:
      ats->fsc = 128;
      break;
    default:
      break;
    }

    ats->ta1.transmitted = (buffer[1] & 0x40) != 0;
    ats->tb1.transmitted = (buffer[1] & 0x20) != 0;
    ats->tc1.transmitted = (buffer[1] & 0x10) != 0;
  }

  uint8_t index = 2;
  if (ats->ta1.transmitted && index < bufferSize) {
    ats->ta1.sameD = (buffer[index] & 0x80) != 0;
    ats->ta1.ds = static_cast<TagBitRates>((buffer[index] & 0x70) >> 4);
    ats->ta1.dr = static_cast<TagBitRates>(buffer[index] & 0x07);
    ++index;
  }
  if (ats->tb1.transmitted && index < bufferSize) {
    ats->tb1.fwi = static_cast<byte>((buffer[index] & 0xF0) >> 4);
    ats->tb1.sfgi = static_cast<byte>(buffer[index] & 0x0F);
    ++index;
  }
  if (ats->tc1.transmitted && index < bufferSize) {
    ats->tc1.supportsCID = (buffer[index] & 0x02) != 0;
    ats->tc1.supportsNAD = (buffer[index] & 0x01) != 0;
  }

  if (bufferSize >= 2) {
    const byte copyLength =
        static_cast<byte>((bufferSize - 2) > sizeof(ats->data)
                              ? sizeof(ats->data)
                              : (bufferSize - 2));
    memcpy(ats->data, buffer, copyLength);
  }

  return STATUS_OK;
}

MFRC522_I2C::StatusCode MFRC522_I2C_Extended::PICC_PPS() {
  byte ppsBuffer[4] = {0xD0, 0x00, 0x00, 0x00};
  byte ppsBufferSize = sizeof(ppsBuffer);
  StatusCode status = PCD_CalculateCRC(ppsBuffer, 2, &ppsBuffer[2]);
  if (status != STATUS_OK) {
    return status;
  }

  status = PCD_TransceiveData(ppsBuffer, sizeof(ppsBuffer), ppsBuffer,
                              &ppsBufferSize, nullptr, 0, true);
  if (status != STATUS_OK) {
    return status;
  }

  PCD_WriteRegister(TxModeReg,
                    static_cast<byte>(PCD_ReadRegister(TxModeReg) | MFRC522_CRC_ENABLE));
  PCD_WriteRegister(RxModeReg,
                    static_cast<byte>(PCD_ReadRegister(RxModeReg) | MFRC522_CRC_ENABLE));
  return STATUS_OK;
}

MFRC522_I2C::StatusCode MFRC522_I2C_Extended::PICC_PPS(
    TagBitRates sendBitRate, TagBitRates receiveBitRate) {
  byte ppsBuffer[5] = {0xD0, 0x11,
                       static_cast<byte>((((sendBitRate & 0x03) << 2) |
                                          (receiveBitRate & 0x03)) &
                                         0xE7),
                       0x00, 0x00};
  byte ppsBufferSize = sizeof(ppsBuffer);

  StatusCode status = PCD_CalculateCRC(ppsBuffer, 3, &ppsBuffer[3]);
  if (status != STATUS_OK) {
    return status;
  }

  status = PCD_TransceiveData(ppsBuffer, sizeof(ppsBuffer), ppsBuffer,
                              &ppsBufferSize, nullptr, 0, true);
  if (status != STATUS_OK) {
    return status;
  }
  if (ppsBufferSize != 3 || ppsBuffer[0] != 0xD0) {
    return STATUS_ERROR;
  }

  byte txMode = static_cast<byte>(PCD_ReadRegister(TxModeReg) & 0x8F);
  byte rxMode = static_cast<byte>(PCD_ReadRegister(RxModeReg) & 0x8F);
  txMode = static_cast<byte>(txMode | ((receiveBitRate & 0x03) << 4) | MFRC522_CRC_ENABLE);
  rxMode = static_cast<byte>((rxMode | ((sendBitRate & 0x03) << 4) | MFRC522_CRC_ENABLE) &
                             0xF0);

  PCD_WriteRegister(TxModeReg, txMode);
  PCD_WriteRegister(RxModeReg, rxMode);

  switch (sendBitRate) {
  case BITRATE_212KBITS:
    PCD_WriteRegister(ModWidthReg, 0x15);
    break;
  case BITRATE_424KBITS:
    PCD_WriteRegister(ModWidthReg, 0x0A);
    break;
  case BITRATE_848KBITS:
    PCD_WriteRegister(ModWidthReg, 0x05);
    break;
  default:
    PCD_WriteRegister(ModWidthReg, 0x26);
    break;
  }

  delayMicroseconds(10);
  return STATUS_OK;
}

MFRC522_I2C::StatusCode MFRC522_I2C_Extended::TCL_Transceive(
    const PcbBlock *send, byte *backData, byte *backLen, byte *backPcb) {
  if (send == nullptr) {
    return STATUS_INVALID;
  }

  PcbBlock current = *send;
  for (byte attempt = 0; attempt < 6; ++attempt) {
    byte outBuffer[FIFO_SIZE] = {0};
    byte outLength = 1;
    outBuffer[0] = current.prologue.pcb;

    if ((current.prologue.pcb & PCB_CID_FOLLOWING) != 0) {
      outBuffer[outLength++] = current.prologue.cid;
    }
    if ((current.prologue.pcb & PCB_NAD_FOLLOWING) != 0) {
      outBuffer[outLength++] = current.prologue.nad;
    }
    if (current.inf.size > 0 && current.inf.data != nullptr) {
      if (static_cast<byte>(outLength + current.inf.size) > FIFO_SIZE) {
        return STATUS_NO_ROOM;
      }
      memcpy(outBuffer + outLength, current.inf.data, current.inf.size);
      outLength = static_cast<byte>(outLength + current.inf.size);
    }

    if ((PCD_ReadRegister(TxModeReg) & MFRC522_CRC_ENABLE) == 0) {
      if (static_cast<byte>(outLength + 2) > FIFO_SIZE) {
        return STATUS_NO_ROOM;
      }
      const StatusCode crcStatus = PCD_CalculateCRC(outBuffer, outLength,
                                                    outBuffer + outLength);
      if (crcStatus != STATUS_OK) {
        return crcStatus;
      }
      outLength = static_cast<byte>(outLength + 2);
    }

    byte inBuffer[FIFO_SIZE] = {0};
    byte inLength = sizeof(inBuffer);
    StatusCode status =
        PCD_TransceiveData(outBuffer, outLength, inBuffer, &inLength);

    // ISO 14443-4 §7.5.5 error recovery: on transceive failure, send R(NAK)
    // to request retransmission. Retry up to kMaxRetries times.
    if (status != STATUS_OK) {
      byte retries = 0;
      while (retries < kMaxRetries) {
        retries++;

        PcbBlock rnak;
        rnak.prologue.pcb = PCB_RNAK;
        if ((send->prologue.pcb & PCB_CID_FOLLOWING) != 0) {
          rnak.prologue.pcb |= PCB_CID_FOLLOWING;
          rnak.prologue.cid = send->prologue.cid;
        }
        if (send->prologue.pcb & PCB_BLOCK_NUMBER) {
          rnak.prologue.pcb |= PCB_BLOCK_NUMBER;
        }
        rnak.inf.size = 0;
        rnak.inf.data = nullptr;

        byte rnakBuffer[FIFO_SIZE] = {0};
        byte rnakLength = 1;
        rnakBuffer[0] = rnak.prologue.pcb;
        if ((rnak.prologue.pcb & PCB_CID_FOLLOWING) != 0) {
          rnakBuffer[rnakLength++] = rnak.prologue.cid;
        }

        if ((PCD_ReadRegister(TxModeReg) & MFRC522_CRC_ENABLE) == 0) {
          if (static_cast<byte>(rnakLength + 2) > FIFO_SIZE) {
            return STATUS_NO_ROOM;
          }
          const StatusCode crcSt = PCD_CalculateCRC(
              rnakBuffer, rnakLength, rnakBuffer + rnakLength);
          if (crcSt != STATUS_OK) {
            return crcSt;
          }
          rnakLength = static_cast<byte>(rnakLength + 2);
        }

        inLength = sizeof(inBuffer);
        status = PCD_TransceiveData(rnakBuffer, rnakLength, inBuffer, &inLength);
        if (status == STATUS_OK && inLength > 0) {
          break;
        }
      }
      if (status != STATUS_OK) {
        return status;
      }
      if (inLength == 0) {
        return STATUS_ERROR;
      }
    }

    byte offset = 1;
    const bool hasCid = (inBuffer[0] & PCB_CID_FOLLOWING) != 0;
    if (hasCid) {
      ++offset;
    }
    if ((inBuffer[0] & PCB_NAD_FOLLOWING) != 0) {
      ++offset;
    }

    if ((PCD_ReadRegister(RxModeReg) & MFRC522_CRC_ENABLE) == 0) {
      if (inLength < static_cast<byte>(offset + 2)) {
        return STATUS_CRC_WRONG;
      }
      byte controlBuffer[2] = {0};
      const StatusCode crcStatus = PCD_CalculateCRC(
          inBuffer, static_cast<byte>(inLength - 2), controlBuffer);
      if (crcStatus != STATUS_OK) {
        return crcStatus;
      }
      if (controlBuffer[0] != inBuffer[inLength - 2] ||
          controlBuffer[1] != inBuffer[inLength - 1]) {
        return STATUS_CRC_WRONG;
      }
      inLength = static_cast<byte>(inLength - 2);
    }

    if (((inBuffer[0] & PCB_TYPE_MASK) == PCB_TYPE_R) && ((inBuffer[0] & PCB_R_NAK) != 0)) {
      return STATUS_MIFARE_NACK;
    }

    const byte infLength =
        inLength > offset ? static_cast<byte>(inLength - offset) : 0;
    if (is_s_block_wtx(inBuffer[0], infLength, inBuffer + offset)) {
      current.prologue.pcb = static_cast<byte>(PCB_SWTX_RSP | (hasCid ? PCB_CID_FOLLOWING : 0x00));
      current.prologue.cid = hasCid ? inBuffer[1] : 0x00;
      current.prologue.nad = 0x00;
      current.inf.size = 1;
      current.inf.data = inBuffer + offset;
      continue;
    }

    if (backPcb != nullptr) {
      *backPcb = inBuffer[0];
    }

    if (backLen != nullptr) {
      if (backData == nullptr && infLength != 0) {
        return STATUS_NO_ROOM;
      }
      if (infLength > *backLen) {
        return STATUS_NO_ROOM;
      }
      if (backData != nullptr && infLength > 0) {
        memcpy(backData, inBuffer + offset, infLength);
      }
      *backLen = infLength;
    }

    return STATUS_OK;
  }

  return STATUS_TIMEOUT;
}

MFRC522_I2C::StatusCode MFRC522_I2C_Extended::TCL_Transceive(
    TagInfo *selectedTag, const byte *sendData, byte sendLen, byte *backData,
    byte *backLen) {
  if (selectedTag == nullptr) {
    return STATUS_INVALID;
  }
  if (selectedTag->isoDepState != IsoDepState::SELECTED) {
    return STATUS_INVALID;
  }

  PcbBlock block;
  block.prologue.pcb = PCB_IBASE;
  if (selectedTag->ats.tc1.supportsCID) {
    block.prologue.pcb |= PCB_CID_FOLLOWING;
    block.prologue.cid = 0x00;
  }
  if (selectedTag->blockNumber) {
    block.prologue.pcb |= PCB_BLOCK_NUMBER;
  }
  block.inf.size = sendLen;
  block.inf.data = sendData;

  byte totalLength = backLen != nullptr ? *backLen : 0;
  byte written = 0;
  byte pcb = 0;
  byte initialCapacity = backLen != nullptr ? *backLen : 0;
  StatusCode status =
      TCL_Transceive(&block, backData, backLen, backLen != nullptr ? &pcb : nullptr);
  if (status != STATUS_OK) {
    return status;
  }

  selectedTag->blockNumber = !selectedTag->blockNumber;
  written = backLen != nullptr ? *backLen : 0;

  while ((pcb & PCB_CHAINING) != 0) {
    byte remaining = totalLength > written ? static_cast<byte>(totalLength - written)
                                           : 0;
    byte chunkLength = remaining;
    status = TCL_TransceiveRBlock(selectedTag, true,
                                  backData != nullptr ? backData + written : nullptr,
                                  backLen != nullptr ? &chunkLength : nullptr);
    if (status != STATUS_OK) {
      return status;
    }

    selectedTag->blockNumber = !selectedTag->blockNumber;
    written = static_cast<byte>(written + chunkLength);
    if (backLen != nullptr) {
      if (written > initialCapacity) {
        return STATUS_NO_ROOM;
      }
      *backLen = written;
    }

    byte nextPcb = 0;
    byte scratchLength = remaining;
    if (backData == nullptr || backLen == nullptr) {
      status = TCL_TransceiveRBlock(selectedTag, true, nullptr, nullptr);
      if (status != STATUS_OK) {
        return status;
      }
      break;
    }

    if (remaining == 0) {
      return STATUS_NO_ROOM;
    }

    PcbBlock ackBlock;
    ackBlock.prologue.pcb = PCB_RACK;
    if (selectedTag->ats.tc1.supportsCID) {
      ackBlock.prologue.pcb |= PCB_CID_FOLLOWING;
      ackBlock.prologue.cid = 0x00;
    }
    if (selectedTag->blockNumber) {
      ackBlock.prologue.pcb |= PCB_BLOCK_NUMBER;
    }
    ackBlock.inf.size = 0;
    ackBlock.inf.data = nullptr;
    status = TCL_Transceive(&ackBlock, backData + written, &scratchLength, &nextPcb);
    if (status != STATUS_OK) {
      return status;
    }
    selectedTag->blockNumber = !selectedTag->blockNumber;
    written = static_cast<byte>(written + scratchLength);
    *backLen = written;
    pcb = nextPcb;
  }

  return STATUS_OK;
}

MFRC522_I2C::StatusCode MFRC522_I2C_Extended::TCL_TransceiveRBlock(
    TagInfo *selectedTag, bool ack, byte *backData, byte *backLen) {
  if (selectedTag == nullptr) {
    return STATUS_INVALID;
  }
  if (selectedTag->isoDepState != IsoDepState::SELECTED) {
    return STATUS_INVALID;
  }

  PcbBlock block;
  block.prologue.pcb = ack ? PCB_RACK : PCB_RNAK;
  if (selectedTag->ats.tc1.supportsCID) {
    block.prologue.pcb |= PCB_CID_FOLLOWING;
    block.prologue.cid = 0x00;
  }
  if (selectedTag->blockNumber) {
    block.prologue.pcb |= PCB_BLOCK_NUMBER;
  }
  block.inf.size = 0;
  block.inf.data = nullptr;

  return TCL_Transceive(&block, backData, backLen, nullptr);
}

MFRC522_I2C::StatusCode MFRC522_I2C_Extended::TCL_Deselect(TagInfo *selectedTag) {
  if (selectedTag == nullptr) {
    return STATUS_INVALID;
  }

  byte outBuffer[2] = {PCB_SDESELECT, 0x00};
  byte outLength = 1;
  if (selectedTag->ats.tc1.supportsCID) {
    outBuffer[0] |= PCB_CID_FOLLOWING;
    outLength = 2;
  }

  byte inBuffer[FIFO_SIZE] = {0};
  byte inLength = sizeof(inBuffer);
  const StatusCode status =
      PCD_TransceiveData(outBuffer, outLength, inBuffer, &inLength);
  if (status == STATUS_OK) {
    selectedTag->isoDepState = IsoDepState::DESELECTED;
    clearTag();
  }
  return status;
}

bool MFRC522_I2C_Extended::PICC_IsNewCardPresent() {
  PCD_WriteRegister(TxModeReg, 0x00);
  PCD_WriteRegister(RxModeReg, 0x00);
  PCD_WriteRegister(ModWidthReg, 0x26);

  byte bufferATQA[2] = {0};
  byte bufferSize = sizeof(bufferATQA);
  StatusCode status = PICC_RequestA(bufferATQA, &bufferSize);
  if (status != STATUS_OK && status != STATUS_COLLISION) {
    bufferSize = sizeof(bufferATQA);
    status = PICC_WakeupA(bufferATQA, &bufferSize);
  }
  if (status == STATUS_OK || status == STATUS_COLLISION) {
    clearTag();
    tag.atqa = static_cast<uint16_t>((bufferATQA[1] << 8) | bufferATQA[0]);
    return true;
  }
  return false;
}

bool MFRC522_I2C_Extended::PICC_ReadCardSerial() {
  const StatusCode status = PICC_Select(&tag.uid);
  uid = tag.uid;
  return status == STATUS_OK;
}

void MFRC522_I2C_Extended::clearTag() {
  MFRC522_I2C::clearTag();
  memset(&tag, 0, sizeof(tag));
  tag.isoDepState = IsoDepState::IDLE;
  tag.ats.fsc = 32;
  tag.ats.ta1.ds = BITRATE_106KBITS;
  tag.ats.ta1.dr = BITRATE_106KBITS;
  tag.ats.tc1.supportsCID = true;
}
