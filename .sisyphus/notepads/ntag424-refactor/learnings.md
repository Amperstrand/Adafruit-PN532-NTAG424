# Learnings

## [2026-04-16] Pre-refactor baseline

- Pre-refactor `ntag424_` reference count in `Adafruit_PN532_NTAG424.h`: **44**
- NTAG424 functions in .cpp begin at line 1342 (`/***** NTAG424 Functions ******/`)
- Lines 873-1341: Mifare Classic/Ultralight — DO NOT TOUCH
- Crypto functions in .cpp: ntag424_random (1354), ntag424_addpadding (1684), ntag424_encrypt (1707, 1728), ntag424_decrypt (1757, 1777), ntag424_cmac_short (1808), ntag424_cmac (1842), ntag424_MAC (1872, 1895), ntag424_derive_session_keys (1962), ntag424_rotl (2067)
- Response helpers (ntag424_response_has_status, ntag424_plain_command_succeeded, ntag424_copy_response_data_if_status) are in `ntag424_changekey_utils.h:25-55` as static inline — NOT in .cpp
- ntag424_apdu_send() is lines 1384-1670 in .cpp — the critical split point
- PN532 framing: lines 1401-1402 prepend PN532_COMMAND_INDATAEXCHANGE + 0x01
- Transport: line 1528 (sendCommandCheckAck), 1543 (readdata), 1553-1554 (response extraction)
- Static helpers (lines 99-135): ntag424_read_simple_full_response, ntag424_iso_select_file, ntag424_plain_status_ok — take Adafruit_PN532*, must change to NTAG424_Reader*
- pn532_packetbuffer[64] at line 96 — global buffer, adapter must use its own local buffer
- Private members _uid, _uidLen, _inListedTag at lines 369-374 — need public getters or friend access
- File layout: Arduino library flat in root directory — all new source files in root, tests in tests/

## [2026-04-16] Architecture decisions

- Reader interface: 3 virtual methods: transceive(), get_uid(), is_tag_present()
- ntag424_SessionType: define in ntag424_crypto.h (Task 1), move to ntag424_core.h later or keep in crypto
- Adapter pattern: forward-declare Adafruit_PN532 in adapter .h, include full header only in .cpp
- Crypto methods stay as thin delegation wrappers on the class (preserving public API)
- Desktop tests use stubs in tests/stubs/ (Arduino.h, Arduino_CRC32.h)

## [2026-04-16] Task 1 extraction notes

- Created `ntag424_crypto.h/.cpp` in project root with 13 free-function crypto declarations and extracted implementations.
- Preserved the existing `ntag424_SessionType` field comments verbatim in the new header and kept the required file header comment.
- Added `__has_include` guards so the extracted crypto module remains LSP-clean on desktop even when Arduino headers are unavailable.
- Session-dependent overloads now take `ntag424_SessionType *session` explicitly; the monolith files were left untouched for later wrapper/removal work.

## [2026-04-16] Task 4 APDU extraction notes

- `ntag424_build_apdu()` must reproduce monolith bytes exactly while shifting the payload start from index 2 to index 0 because PN532 transport framing is excluded from the new module.
- `ntag424_process_response()` keeps CMAC verification and FULL-mode decryption together and increments `session->cmd_counter` only after processing succeeds, matching the monolith behavior.
- Reader-facing helpers now depend on `NTAG424_Reader::transceive()` plus explicit `session` and `TI` inputs; they no longer depend on `Adafruit_PN532`, PN532 framing, or packetbuffer globals.

## [2026-04-16] Task 6 core extraction notes

- `ntag424_core.h` mirrors the monolith NTAG424 constants plus `ntag424_VersionInfoType` and `ntag424_FileSettings`, but keeps transport dependencies limited to `NTAG424_Reader`, `ntag424_apdu`, and `ntag424_crypto`.
- The five tightly coupled protocol paths (`Authenticate`, `GetVersion`, `ReadData`, `ISOReadFile`, `ISOReadBinary`) now build raw APDU bytes and call `reader->transceive()` directly, leaving PN532 `InDataExchange` framing to adapters.
- Commit scope for Task 6 should stay limited to the new core files, task evidence, and notepad update because the repository already contains unrelated modified/untracked files.

## [2026-04-16] Task 7 adapter notes

- Added `PN532_NTAG424_Adapter` as a minimal `NTAG424_Reader` implementation with only `transceive()`, `get_uid()`, and `is_tag_present()`.
- Adapter header forward-declares `Adafruit_PN532`; only the `.cpp` pulls in `Adafruit_PN532_NTAG424.h` to avoid circular includes.
- `transceive()` mirrors the monolith InDataExchange framing and response extraction exactly, but uses local `pn532_frame` and `resp_buf` arrays instead of the global packet buffer.
- Added public `getUID()` and `isTagPresent()` accessors on `Adafruit_PN532` so the adapter can expose UID/presence without taking ownership of reader state.

## [2026-04-16] Task 8 thin-wrapper notes

- `Adafruit_PN532_NTAG424.h` now includes `ntag424_core.h` and `pn532_ntag424_adapter.h`, removes duplicated NTAG424 constants/structs, and keeps the public NTAG424 API plus public state members intact.
- Every NTAG424 method body in `Adafruit_PN532_NTAG424.cpp` is now a thin wrapper that delegates to `::ntag424_*` free functions or uses `_ntag424_adapter.transceive()` + `ntag424_build_apdu()` + `ntag424_process_response()` for `ntag424_apdu_send()`.
- All four constructors now initialize `_ntag424_adapter(this)` in their member initializer lists, while non-NTAG424 regions before line 1342 and after line 3523 remain untouched.
- Desktop crypto tests required an internal non-Arduino CRC32 fallback in `ntag424_crypto.cpp` so the host build no longer depends on linking zlib for that target.
