#ifndef TESTS_MOCK_READER_H
#define TESTS_MOCK_READER_H

#include <cstdint>
#include <cstring>
#include <vector>

#include "../ntag424_reader.h"

class ScriptedMockReader : public NTAG424_Reader {
public:
  struct ScriptStep {
    std::vector<uint8_t> response;
    bool is_wtx = false;
  };

  uint8_t transceive(const uint8_t *send, uint8_t sendLength,
                     uint8_t *response, uint8_t responseMaxLength) override {
    ++transceive_call_count_;

    sent_frames_.emplace_back();
    if (send != nullptr && sendLength > 0) {
      sent_frames_.back().assign(send, send + sendLength);
    }

    if (fail_on_call_ != 0 && transceive_call_count_ == fail_on_call_) {
      return 0;
    }

    while (next_step_index_ < script_.size()) {
      const ScriptStep &step = script_[next_step_index_++];
      if (step.is_wtx) {
        ++wtx_count_;
        continue;
      }

      if (response == nullptr || step.response.size() > responseMaxLength) {
        return 0;
      }

      if (!step.response.empty()) {
        std::memcpy(response, step.response.data(), step.response.size());
      }
      return static_cast<uint8_t>(step.response.size());
    }

    return 0;
  }

  uint8_t get_uid(uint8_t *uid, uint8_t uidMaxLength) override {
    if (uid == nullptr || uid_.size() > uidMaxLength) {
      return 0;
    }

    if (!uid_.empty()) {
      std::memcpy(uid, uid_.data(), uid_.size());
    }
    return static_cast<uint8_t>(uid_.size());
  }

  bool is_tag_present() override { return tag_present_; }

  void queue_response(const uint8_t *data, uint8_t length) {
    ScriptStep step;
    if (data != nullptr && length > 0) {
      step.response.assign(data, data + length);
    }
    script_.push_back(step);
  }

  void queue_wtx(uint8_t wtxm) {
    ScriptStep step;
    step.is_wtx = true;
    step.response.push_back(0xF2);
    step.response.push_back(wtxm);
    script_.push_back(step);
  }

  void clear_script() {
    script_.clear();
    next_step_index_ = 0;
    wtx_count_ = 0;
  }

  void clear_sent_frames() { sent_frames_.clear(); }

  void reset() {
    clear_script();
    clear_sent_frames();
    fail_on_call_ = 0;
    transceive_call_count_ = 0;
  }

  void set_fail_on_call(uint32_t call_index) { fail_on_call_ = call_index; }

  void set_uid(const uint8_t *uid, uint8_t uidLength) {
    uid_.clear();
    if (uid != nullptr && uidLength > 0) {
      uid_.assign(uid, uid + uidLength);
    }
  }

  void set_tag_present(bool present) { tag_present_ = present; }

  const std::vector<std::vector<uint8_t>> &sent_frames() const {
    return sent_frames_;
  }

  uint32_t transceive_call_count() const { return transceive_call_count_; }
  uint32_t wtx_count() const { return wtx_count_; }
  uint32_t scripted_steps_remaining() const {
    return static_cast<uint32_t>(script_.size() - next_step_index_);
  }

private:
  std::vector<ScriptStep> script_;
  std::vector<std::vector<uint8_t>> sent_frames_;
  std::vector<uint8_t> uid_;
  uint32_t next_step_index_ = 0;
  uint32_t fail_on_call_ = 0;
  uint32_t transceive_call_count_ = 0;
  uint32_t wtx_count_ = 0;
  bool tag_present_ = true;
};

#endif
