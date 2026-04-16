#ifndef MFRC522_NTAG424_ADAPTER_H
#define MFRC522_NTAG424_ADAPTER_H

#include "MFRC522_I2C_Extended.h"
#include "ntag424_reader.h"

class MFRC522_NTAG424_Adapter : public NTAG424_Reader {
public:
  explicit MFRC522_NTAG424_Adapter(MFRC522_I2C_Extended *reader)
      : _reader(reader) {}

  uint8_t transceive(const uint8_t *send, uint8_t sendLength,
                     uint8_t *response, uint8_t responseMaxLength) override;
  uint8_t get_uid(uint8_t *uid, uint8_t uidMaxLength) override;
  bool is_tag_present() override;

private:
  MFRC522_I2C_Extended *_reader;
};

#endif
