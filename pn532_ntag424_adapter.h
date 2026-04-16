#ifndef PN532_NTAG424_ADAPTER_H
#define PN532_NTAG424_ADAPTER_H

#include "ntag424_reader.h"

class Adafruit_PN532;

class PN532_NTAG424_Adapter : public NTAG424_Reader {
public:
  explicit PN532_NTAG424_Adapter(Adafruit_PN532 *pn532) : _pn532(pn532) {}

  uint8_t transceive(const uint8_t *send, uint8_t sendLength,
                     uint8_t *response, uint8_t responseMaxLength) override;
  uint8_t get_uid(uint8_t *uid, uint8_t uidMaxLength) override;
  bool is_tag_present() override;

private:
  Adafruit_PN532 *_pn532;
};

#endif
