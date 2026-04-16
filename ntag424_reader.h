// Minimal abstract reader interface for NTAG424 protocol layer
#ifndef NTAG424_READER_H
#define NTAG424_READER_H

#include <stdint.h>

class NTAG424_Reader {
public:
  virtual ~NTAG424_Reader() = default;

  // Send raw APDU bytes and receive response. Returns response length, 0 on error.
  virtual uint8_t transceive(const uint8_t *send, uint8_t sendLength,
                             uint8_t *response, uint8_t responseMaxLength) = 0;

  // Get the UID of the currently selected tag. Returns UID length, 0 if no tag.
  virtual uint8_t get_uid(uint8_t *uid, uint8_t uidMaxLength) = 0;

  // Check if a tag is currently in the RF field and selected.
  virtual bool is_tag_present() = 0;
};

#endif // NTAG424_READER_H
