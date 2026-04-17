# NTAG424 Separation Refactor — Prepare for Reader-Agnostic Extraction

## TL;DR

> **Quick Summary**: Refactor the monolithic `Adafruit_PN532_NTAG424` class to separate NTAG424 protocol/crypto/APDU logic from PN532 transport details, using a minimal internal reader interface. This is preparation for future extraction — no new hardware support, no API changes.
>
> **Deliverables**:
> - `ntag424_crypto.h/.cpp` — all NTAG424 crypto primitives (encrypt, decrypt, CMAC, MAC, session key derivation, CRC32, padding, random)
> - `ntag424_apdu.h/.cpp` — APDU construction, response parsing, MAC/ENC wrapping (reader-agnostic portion of current `ntag424_apdu_send`)
> - `ntag424_reader.h` — minimal abstract reader interface (3 virtual methods)
> - `ntag424_core.h/.cpp` — NTAG424 protocol logic, session state structs, command orchestration
> - `pn532_ntag424_adapter.h/.cpp` — PN532 implementation of reader interface
> - Modified `Adafruit_PN532_NTAG424.h/.cpp` — thin compatibility wrapper delegating to core via adapter
> - Desktop tests for crypto and APDU modules
>
> **Estimated Effort**: Large
> **Parallel Execution**: YES — 4 waves
> **Critical Path**: Task 1 → Task 3 → Task 5 → Task 7 → Task 8 → Task 9 → Final Verification

---

## Context

### Original Request
Refactor the `Adafruit_PN532_NTAG424` codebase to prepare for a later extraction of all NTAG424-specific logic into a reader-agnostic layer that can also support `MFRC522` in the future. Before making changes, produce a concise refactor plan mapping current PN532+NTAG424 code into: transport/reader concerns, NTAG424 protocol concerns, crypto/session concerns, thin compatibility wrappers. Then implement the refactor.

### Interview Summary
**Key Discussions**:
- The codebase is a monolithic Arduino library: single `Adafruit_PN532` class in `Adafruit_PN532_NTAG424.h/.cpp` (~4200 lines)
- Two coupling patterns: 13 cleanly-delegated methods (via `ntag424_apdu_send()`) and 5 tightly-coupled methods (directly use `pn532_packetbuffer`, `sendCommandCheckAck()`, `readdata()`)
- `ntag424_apdu_send()` (lines 1384-1670) is the critical bridge mixing APDU construction with PN532 framing
- NTAG424 crypto functions are all reader-agnostic and safe to extract
- Existing well-separated files: `aescmac.h/.cpp`, `ntag424_changekey_utils.h`
- No automated test suite; Arduino compilation is the main check
- Arduino library layout: all files flat in root directory
- ESP32-only due to mbedtls dependency

**Research Findings**:
- `pn532_packetbuffer[64]` is a file-scope global shared between PN532 and NTAG424 code — adapter must provide buffer access or new modules use local buffers
- Static helpers (`ntag424_read_simple_full_response`, `ntag424_iso_select_file`, `ntag424_plain_status_ok`) take `Adafruit_PN532*` — must be routed through interface
- Session state structs (`ntag424_SessionType`, `ntag424_VersionInfoType`, `ntag424_FileSettings`) are reader-agnostic data currently inside the class
- Multi-frame exchanges (Authenticate: 3-step, GetVersion: 3-frame) need multiple `transceive_apdu()` calls, not single-call wrapping
- `esp_random()` is ESP32-specific, comes along with crypto extraction — keep as-is per constraints

### Metis Review
**Identified Gaps** (addressed):
- `pn532_packetbuffer` ownership: Adapter owns it; extracted modules use local stack buffers (no shared global needed)
- mbedtls dependency: Keep as direct dependency of `ntag424_crypto` — no abstraction (per constraint "no speculative abstractions")
- Include graph: `ntag424_crypto.h` → `aescmac.h` + mbedtls; `ntag424_apdu.h` → `ntag424_crypto.h`; `ntag424_core.h` → both; `ntag424_reader.h` → standalone
- Example sketch compatibility: `#include <Adafruit_PN532_NTAG424.h>` continues to pull in everything — no sketch changes
- Public API preservation: All 66 public method signatures preserved identically
- Desktop test pattern: Follow `tests/test_changekey_helpers.cpp` structure (plain g++ compilation, no framework)

---

## Work Objectives

### Core Objective
Restructure the monolithic `Adafruit_PN532_NTAG424` class so that NTAG424 protocol, crypto, and APDU logic lives in dedicated modules with a minimal reader interface, making future extraction into a standalone reader-agnostic library straightforward.

### Concrete Deliverables
- 6 new source files in project root (3 `.h/.cpp` pairs + 1 header-only + 1 adapter pair)
- 2 new desktop test files in `tests/`
- Desktop compilation stubs in `tests/stubs/` (Arduino.h, Arduino_CRC32.h, optionally mbedtls/aes.h)
- Modified `Adafruit_PN532_NTAG424.h/.cpp` as thin wrapper
- Zero changes to example sketches
- Zero changes to public API signatures

### Definition of Done
- [ ] `arduino-cli compile --fqbn esp32:esp32:esp32 examples/ntag424_examples/ntag424_examples.ino` succeeds with zero errors (or equivalent g++ syntax check if arduino-cli unavailable)
- [ ] `arduino-cli compile --fqbn esp32:esp32:esp32 examples/ntag424_isoreadfile/ntag424_isoreadfile.ino` succeeds
- [ ] Existing desktop test `tests/test_changekey_helpers.cpp` still compiles and passes
- [ ] New desktop tests for crypto and APDU modules compile and pass
- [ ] All public methods on `Adafruit_PN532` class have identical signatures
- [ ] All new files are in project root directory (flat layout)

### Must Have
- Reader interface with exactly 3 methods: `transceive()`, `get_uid()`, `is_tag_present()`
- Crypto module containing all `ntag424_encrypt/decrypt/cmac/MAC/derive_session_keys/crc32/addpadding/rotl/random`
- APDU module containing reader-agnostic APDU construction and response parsing from `ntag424_apdu_send()`
- Core module containing protocol-level command orchestration and session state structs
- PN532 adapter implementing reader interface using existing PN532 transport methods
- Backward-compatible wrapper preserving all public method signatures

### Must NOT Have (Guardrails)
- No MFRC522 code or any second reader implementation
- No speculative abstractions beyond the 3-method reader interface (no `configure()`, `reset()`, `get_firmware_version()`)
- No changes to any method that doesn't have `ntag424` in its name (PN532 base code untouched)
- No changes to example sketches — `#include <Adafruit_PN532_NTAG424.h>` must work unchanged
- No abstraction over mbedtls or `esp_random()` — keep direct dependencies
- No subdirectories for new **library source** files — everything stays in root (test files go in existing `tests/` directory, stubs in `tests/stubs/`)
- No new error enums or return type changes — if it returns `bool` today, it returns `bool` after
- No "while we're here" improvements — purely mechanical extraction
- No adding Doxygen/JSDoc to every function — at most 1-2 line file-level header comments
- No merging `aescmac.h/.cpp` into the crypto module — leave as-is, include from crypto
- No renaming the library or changing `library.properties` metadata

---

## Verification Strategy

> **ZERO HUMAN INTERVENTION** — ALL verification is agent-executed. No exceptions.

### Test Decision
- **Infrastructure exists**: YES — desktop C++ tests in `tests/` (no framework, plain g++)
- **Automated tests**: Tests-after (new desktop tests for crypto/APDU modules)
- **Framework**: None — plain g++ with assertions, following `test_changekey_helpers.cpp` pattern
- **Arduino compilation**: Primary verification via `arduino-cli compile` or `g++ -fsyntax-only` equivalent

### QA Policy
Every task MUST include agent-executed QA scenarios.
Evidence saved to `.sisyphus/evidence/task-{N}-{scenario-slug}.{ext}`.

- **Compilation**: Use Bash — `arduino-cli compile` or `g++ -fsyntax-only` with ESP32/Arduino includes
- **Desktop tests**: Use Bash — `g++ -std=c++17 -o test tests/test_file.cpp && ./test`
- **API verification**: Use Bash — grep/diff public method signatures before and after

### Compilation Verification Command
If `arduino-cli` is available:
```bash
arduino-cli compile --fqbn esp32:esp32:esp32 examples/ntag424_examples/ntag424_examples.ino
```
If not, verify include structure and syntax:
```bash
# Check that all new headers are parseable
for f in ntag424_crypto.h ntag424_apdu.h ntag424_reader.h ntag424_core.h pn532_ntag424_adapter.h; do
  g++ -std=c++17 -fsyntax-only -I. "$f" 2>&1 || echo "FAIL: $f"
done
```

---

## Execution Strategy

### Parallel Execution Waves

```
Wave 1 (Foundation — independent modules, MAX PARALLEL):
├── Task 1: Extract ntag424_crypto.h/.cpp [deep]
├── Task 2: Create ntag424_reader.h interface [quick]
└── Task 3: Desktop tests for crypto module [unspecified-high]

Wave 2 (APDU layer — depends on crypto):
├── Task 4: Extract ntag424_apdu.h/.cpp [deep]
└── Task 5: Desktop tests for APDU module [unspecified-high]

Wave 3 (Core + Adapter — depends on APDU + reader interface):
├── Task 6: Extract ntag424_core.h/.cpp with session structs [deep]
├── Task 7: Create pn532_ntag424_adapter.h/.cpp [deep]

Wave 4 (Integration — depends on ALL above):
├── Task 8: Convert Adafruit_PN532_NTAG424 to thin wrapper [deep]
└── Task 9: Full regression verification [unspecified-high]

Wave FINAL (4 parallel reviews, then user okay):
├── Task F1: Plan compliance audit (oracle)
├── Task F2: Code quality review (unspecified-high)
├── Task F3: Real manual QA (unspecified-high)
└── Task F4: Scope fidelity check (deep)
-> Present results -> Get explicit user okay
```

### Dependency Matrix

| Task | Depends On | Blocks | Wave |
|------|-----------|--------|------|
| 1 (crypto) | — | 3, 4, 6 | 1 |
| 2 (reader interface) | — | 6, 7 | 1 |
| 3 (crypto tests) | 1 | — | 1 |
| 4 (APDU) | 1 | 5, 6, 8 | 2 |
| 5 (APDU tests) | 4 | — | 2 |
| 6 (core) | 1, 2, 4 | 8 | 3 |
| 7 (adapter) | 2 | 8 | 3 |
| 8 (wrapper) | 4, 6, 7 | 9 | 4 |
| 9 (regression) | 8 | F1-F4 | 4 |

### Agent Dispatch Summary

- **Wave 1**: 3 tasks — T1 → `deep`, T2 → `quick`, T3 → `unspecified-high`
- **Wave 2**: 2 tasks — T4 → `deep`, T5 → `unspecified-high`
- **Wave 3**: 2 tasks — T6 → `deep`, T7 → `deep`
- **Wave 4**: 2 tasks — T8 → `deep`, T9 → `unspecified-high`
- **FINAL**: 4 tasks — F1 → `oracle`, F2 → `unspecified-high`, F3 → `unspecified-high`, F4 → `deep`

---

## TODOs

- [x] 1. Extract `ntag424_crypto.h/.cpp` from monolith

  **What to do**:
  - Create `ntag424_crypto.h` in project root with declarations for all crypto functions as free functions (not class methods). These currently live as `Adafruit_PN532` methods in `Adafruit_PN532_NTAG424.h` lines 226-250:
    - `ntag424_crc32(uint8_t *data, uint8_t datalength)` → free function (line 226)
    - `ntag424_addpadding(uint8_t inputlength, uint8_t paddinglength, uint8_t *buffer)` → free function (line 227-228)
    - `ntag424_encrypt(uint8_t *key, uint8_t length, uint8_t *input, uint8_t *output)` → free function (lines 229-230)
    - `ntag424_encrypt(uint8_t *key, uint8_t *iv, uint8_t length, uint8_t *input, uint8_t *output)` → free function (lines 231-232)
    - `ntag424_decrypt(uint8_t *key, uint8_t length, uint8_t *input, uint8_t *output)` → free function (lines 233-234)
    - `ntag424_decrypt(uint8_t *key, uint8_t *iv, uint8_t length, uint8_t *input, uint8_t *output)` → free function (lines 235-236)
    - `ntag424_cmac_short(uint8_t *key, uint8_t *input, uint8_t length, uint8_t *cmac)` → free function (lines 237-238)
    - `ntag424_cmac(uint8_t *key, uint8_t *input, uint8_t length, uint8_t *cmac)` → free function (lines 239-240)
    - `ntag424_MAC(uint8_t *cmd, uint8_t *cmdheader, uint8_t cmdheader_length, uint8_t *cmddata, uint8_t cmddata_length, uint8_t *signature)` — this overload uses session state, so signature becomes: add `ntag424_SessionType *session` as first param (lines 241-243)
    - `ntag424_MAC(uint8_t *key, uint8_t *cmd, uint8_t *cmdheader, uint8_t cmdheader_length, uint8_t *cmddata, uint8_t cmddata_length, uint8_t *signature)` → free function (lines 244-246)
    - `ntag424_random(uint8_t *output, uint8_t bytecount)` → free function (line 247)
    - `ntag424_derive_session_keys(uint8_t *key, uint8_t *RndA, uint8_t *RndB)` — uses session state, add `ntag424_SessionType *session` param (line 248)
    - `ntag424_rotl(uint8_t *input, uint8_t *output, uint8_t bufferlen, uint8_t rotation)` → free function (lines 249-250)
  - Create `ntag424_crypto.cpp` with implementations moved from `Adafruit_PN532_NTAG424.cpp`. The implementations are at these approximate locations:
    - `ntag424_crc32`: lines ~873-896
    - `ntag424_addpadding`: lines ~898-916
    - `ntag424_encrypt` (2 overloads): lines ~918-972
    - `ntag424_decrypt` (2 overloads): lines ~974-1028
    - `ntag424_cmac_short`: lines ~1030-1070
    - `ntag424_cmac`: lines ~1072-1110
    - `ntag424_MAC` (2 overloads): lines ~1112-1190
    - `ntag424_random`: lines ~1192-1210
    - `ntag424_derive_session_keys`: lines ~1212-1300
    - `ntag424_rotl`: lines ~1302-1330
  - The `ntag424_SessionType` struct definition must be available to crypto functions that need it. For now, define it in `ntag424_crypto.h` (it will later move to `ntag424_core.h` in Task 6, but crypto needs it first). Include the struct definition from the header at lines 305-311.
  - Include dependencies: `#include "aescmac.h"`, `#include "mbedtls/aes.h"`, `#include <Arduino.h>`, `#include <Arduino_CRC32.h>`
  - Remove the `Adafruit_PN532::` class prefix from all function implementations — they become free functions
  - For functions that access `this->ntag424_Session` (MAC with session, derive_session_keys), change to take `ntag424_SessionType*` parameter
  - **DO NOT** remove these functions from `Adafruit_PN532_NTAG424.h/.cpp` yet — that happens in Task 8
  - Add include guard: `#ifndef NTAG424_CRYPTO_H` / `#define NTAG424_CRYPTO_H`
  - 1-line file header comment: `// NTAG424 cryptographic primitives (reader-agnostic)`

  **Must NOT do**:
  - Do not modify `aescmac.h/.cpp` — leave as-is, include from crypto
  - Do not abstract mbedtls or esp_random() behind interfaces
  - Do not change any crypto algorithm logic — byte-for-byte identical behavior
  - Do not add Doxygen to every function
  - Do not create subdirectories

  **Recommended Agent Profile**:
  - **Category**: `deep`
    - Reason: Careful extraction from monolith requiring exact line-by-line code movement, signature transformation (class method → free function + session pointer), and include dependency management
  - **Skills**: []
  - **Skills Evaluated but Omitted**:
    - `git-master`: Not needed — single commit, straightforward

  **Parallelization**:
  - **Can Run In Parallel**: YES
  - **Parallel Group**: Wave 1 (with Tasks 2, 3)
  - **Blocks**: Tasks 3, 4, 6
  - **Blocked By**: None (can start immediately)

  **References**:

  **Pattern References** (existing code to follow):
  - `aescmac.h` / `aescmac.cpp` — Follow this pattern for header/implementation separation of crypto primitives. Note the include guard style and minimal header comment.
  - `ntag424_changekey_utils.h` — Follow this for header-only pure-logic helper style (though crypto will have .cpp too)

  **API/Type References** (contracts to implement against):
  - `Adafruit_PN532_NTAG424.h:226-250` — Exact method signatures to convert to free functions
  - `Adafruit_PN532_NTAG424.h:305-311` — `ntag424_SessionType` struct definition to copy
  - `Adafruit_PN532_NTAG424.h:303` — `NTAG424_SESSION_KEYSIZE` constant needed by session struct

  **Implementation References** (source code to extract):
  - `Adafruit_PN532_NTAG424.cpp:1354-2067` (approximate) — Crypto function implementations to extract. NOTE: lines 873-1341 are Mifare Classic/Ultralight — do NOT touch those. NTAG424 functions begin at line 1342. Crypto-related functions in this range: `ntag424_random` (1354), `ntag424_addpadding` (1684), `ntag424_encrypt` (1707, 1728), `ntag424_decrypt` (1757, 1777), `ntag424_cmac_short` (1808), `ntag424_cmac` (1842), `ntag424_MAC` (1872, 1895), `ntag424_derive_session_keys` (1962), `ntag424_rotl` (2067). Use `grep -n` to verify exact lines before moving.

  **External References**:
  - None needed — this is pure extraction, no new APIs

  **WHY Each Reference Matters**:
  - The header signatures (lines 226-250) define the exact API contract — every free function must accept identical parameters (plus session pointer where needed)
  - The session struct (lines 305-311) must be copied verbatim — crypto functions that take session pointers depend on its exact layout
  - The aescmac pattern shows how this project separates crypto concerns — match the style

  **Acceptance Criteria**:

  - [ ] `ntag424_crypto.h` exists in project root with include guard
  - [ ] `ntag424_crypto.cpp` exists in project root
  - [ ] All 13 crypto functions declared as free functions (not class methods)
  - [ ] `ntag424_SessionType` struct defined in `ntag424_crypto.h`
  - [ ] Functions using session state take `ntag424_SessionType*` parameter
  - [ ] `#include "ntag424_crypto.h"` compiles without errors (syntax check)
  - [ ] No `Adafruit_PN532::` prefix in `ntag424_crypto.cpp`

  **QA Scenarios (MANDATORY):**

  ```
  Scenario: Crypto header compiles standalone
    Tool: Bash
    Preconditions: ntag424_crypto.h and ntag424_crypto.cpp exist in project root
    Steps:
      1. Run: g++ -std=c++17 -fsyntax-only -I. ntag424_crypto.h 2>&1
         (May fail due to Arduino.h — that's expected on desktop. Check for OUR errors only)
      2. Run: grep -c 'ntag424_encrypt\|ntag424_decrypt\|ntag424_cmac\|ntag424_MAC\|ntag424_crc32\|ntag424_addpadding\|ntag424_rotl\|ntag424_random\|ntag424_derive_session_keys' ntag424_crypto.h
      3. Assert count >= 13 (all functions declared)
      4. Run: grep -c 'Adafruit_PN532' ntag424_crypto.h ntag424_crypto.cpp
      5. Assert count = 0 (no PN532 coupling)
    Expected Result: All functions declared, zero PN532 references in crypto module
    Failure Indicators: Any function missing from header, or any Adafruit_PN532 reference found
    Evidence: .sisyphus/evidence/task-1-crypto-header-check.txt

  Scenario: Session struct is defined correctly
    Tool: Bash
    Preconditions: ntag424_crypto.h exists
    Steps:
      1. Run: grep -A 10 'struct ntag424_SessionType' ntag424_crypto.h
      2. Assert output contains: authenticated, cmd_counter, session_key_enc, session_key_mac
      3. Run: grep 'NTAG424_SESSION_KEYSIZE' ntag424_crypto.h
      4. Assert the constant is defined
    Expected Result: Struct matches original definition from Adafruit_PN532_NTAG424.h:305-311
    Failure Indicators: Missing struct members or missing keysize constant
    Evidence: .sisyphus/evidence/task-1-session-struct.txt

  Scenario: No duplicate definitions
    Tool: Bash
    Preconditions: Both crypto files and original files exist
    Steps:
      1. The original Adafruit_PN532_NTAG424.h/.cpp still has the methods (not removed yet — Task 8)
      2. Verify ntag424_crypto.cpp does NOT include Adafruit_PN532_NTAG424.h
      3. Run: grep '#include.*Adafruit_PN532' ntag424_crypto.cpp
      4. Assert zero matches
    Expected Result: Crypto module is self-contained, no include of the monolith header
    Failure Indicators: Include of Adafruit_PN532_NTAG424.h found in crypto files
    Evidence: .sisyphus/evidence/task-1-no-circular.txt
  ```

  **Evidence to Capture:**
  - [ ] task-1-crypto-header-check.txt
  - [ ] task-1-session-struct.txt
  - [ ] task-1-no-circular.txt

  **Commit**: YES
  - Message: `refactor: extract ntag424_crypto.h/.cpp from monolith`
  - Files: `ntag424_crypto.h`, `ntag424_crypto.cpp`
  - Pre-commit: syntax check passes

- [x] 2. Create `ntag424_reader.h` abstract interface

  **What to do**:
  - Create `ntag424_reader.h` in project root — a header-only abstract class defining the minimal reader interface
  - The interface has exactly 3 pure virtual methods:
    ```cpp
    class NTAG424_Reader {
    public:
      virtual ~NTAG424_Reader() = default;

      // Send raw APDU bytes (already framed with CLA/INS/P1/P2/Lc/data/Le)
      // and receive response bytes. Returns response length, 0 on error.
      virtual uint8_t transceive(const uint8_t *send, uint8_t sendLength,
                                 uint8_t *response, uint8_t responseMaxLength) = 0;

      // Get the UID of the currently selected tag. Returns UID length, 0 if no tag.
      virtual uint8_t get_uid(uint8_t *uid, uint8_t uidMaxLength) = 0;

      // Check if a tag is currently in the RF field and selected.
      virtual bool is_tag_present() = 0;
    };
    ```
  - The `transceive()` method handles the reader-specific framing (e.g., PN532 prepends `0x40 0x01` for InDataExchange). The caller passes raw ISO 7816-4 APDU bytes; the reader wraps them in its transport protocol.
  - Include guard: `#ifndef NTAG424_READER_H` / `#define NTAG424_READER_H`
  - Only include `<stdint.h>` — no Arduino.h dependency (the interface is pure C++)
  - 1-line header comment: `// Minimal abstract reader interface for NTAG424 protocol layer`
  - Keep it minimal — no `configure()`, no `reset()`, no `get_firmware_version()`. Exactly 3 methods.

  **Must NOT do**:
  - Do not add any methods beyond the 3 specified
  - Do not add any concrete implementation in this file
  - Do not include Arduino.h or any PN532-specific headers
  - Do not add MFRC522 or any second reader stub

  **Recommended Agent Profile**:
  - **Category**: `quick`
    - Reason: Small single-file creation, ~30 lines, no complex logic
  - **Skills**: []

  **Parallelization**:
  - **Can Run In Parallel**: YES
  - **Parallel Group**: Wave 1 (with Tasks 1, 3)
  - **Blocks**: Tasks 6, 7
  - **Blocked By**: None (can start immediately)

  **References**:

  **Pattern References**:
  - `ntag424_changekey_utils.h` — Follow header-only pattern with include guard
  - `Adafruit_PN532_NTAG424.h:196-197` — `inDataExchange()` signature shows the transport-level API the adapter will wrap

  **API/Type References**:
  - `Adafruit_PN532_NTAG424.cpp:1401-1402` — Lines where `PN532_COMMAND_INDATAEXCHANGE` and tag number `0x01` are prepended — this is what `transceive()` abstracts away
  - `Adafruit_PN532_NTAG424.h:371-372` — `_uid` and `_uidLen` private members — `get_uid()` wraps access to these

  **WHY Each Reference Matters**:
  - The InDataExchange framing (lines 1401-1402) defines exactly what the reader-specific transport does — `transceive()` must accept the APDU bytes AFTER CLA/INS/P1/P2 and handle the reader-specific wrapping internally
  - The uid members show what data `get_uid()` needs to expose

  **Acceptance Criteria**:

  - [ ] `ntag424_reader.h` exists in project root
  - [ ] Contains exactly 3 pure virtual methods: `transceive`, `get_uid`, `is_tag_present`
  - [ ] Virtual destructor is defaulted
  - [ ] No Arduino.h dependency — only `<stdint.h>`
  - [ ] No concrete implementations
  - [ ] Compiles with: `g++ -std=c++17 -fsyntax-only ntag424_reader.h`

  **QA Scenarios (MANDATORY):**

  ```
  Scenario: Reader interface compiles on desktop without Arduino
    Tool: Bash
    Preconditions: ntag424_reader.h exists in project root
    Steps:
      1. Run: g++ -std=c++17 -fsyntax-only ntag424_reader.h 2>&1
      2. Assert exit code 0 (no errors)
      3. Run: grep -c 'virtual.*= 0' ntag424_reader.h
      4. Assert count = 3 (exactly 3 pure virtual methods)
      5. Run: grep -c 'Arduino\|PN532\|Adafruit' ntag424_reader.h
      6. Assert count = 0 (no hardware dependencies)
    Expected Result: Clean compilation, exactly 3 pure virtuals, zero hardware references
    Failure Indicators: Compilation error, wrong virtual count, hardware dependencies found
    Evidence: .sisyphus/evidence/task-2-reader-interface.txt

  Scenario: Interface is minimal (no scope creep)
    Tool: Bash
    Preconditions: ntag424_reader.h exists
    Steps:
      1. Run: grep -c 'virtual' ntag424_reader.h
      2. Assert count = 4 (3 pure virtuals + 1 virtual destructor)
      3. Run: wc -l < ntag424_reader.h
      4. Assert line count < 40 (minimal file)
    Expected Result: Exactly 4 virtual declarations (3 methods + destructor), under 40 lines
    Failure Indicators: More than 4 virtuals (scope creep) or excessively large file
    Evidence: .sisyphus/evidence/task-2-minimal-check.txt
  ```

  **Evidence to Capture:**
  - [ ] task-2-reader-interface.txt
  - [ ] task-2-minimal-check.txt

  **Commit**: YES
  - Message: `refactor: add ntag424_reader.h abstract interface`
  - Files: `ntag424_reader.h`
  - Pre-commit: `g++ -std=c++17 -fsyntax-only ntag424_reader.h`

- [x] 3. Desktop tests for ntag424_crypto module

  **What to do**:
  - **First, create desktop compilation stubs** in `tests/stubs/` directory. These are minimal header-only stubs enabling desktop `g++` compilation of Arduino/ESP32-dependent source files:
    - `tests/stubs/Arduino.h` — provides `uint8_t`, `memcpy`, `memset`, `Serial` stub (no-op print), `delay()` stub, `LOW`/`HIGH`/`INPUT`/`OUTPUT` constants. Just enough to make `aescmac.cpp` and `ntag424_crypto.cpp` compile.
    - `tests/stubs/Arduino_CRC32.h` — provides a minimal `Arduino_CRC32` class with a `calc()` method that wraps a standard CRC32 implementation (use `<crc32>` or a simple lookup table implementation).
    - `tests/stubs/mbedtls/aes.h` — if system mbedtls is not available, provide a stub that wraps the system OpenSSL AES or a minimal AES-128 implementation. Check if `mbedtls` is installed with `pkg-config --exists mbedtls` first — if available, this stub is not needed.
  - These stubs are ONLY for desktop test compilation. They do NOT modify the main library source files.
  - **Compilation command** for all desktop tests uses `-I tests/stubs` to pick up stubs before system headers:
    ```bash
    g++ -std=c++17 -I tests/stubs -I. -o test_crypto tests/test_ntag424_crypto.cpp ntag424_crypto.cpp aescmac.cpp -lmbedcrypto 2>&1
    ```
    If `-lmbedcrypto` is not available, compile with the mbedtls stub instead.
  - Then create `tests/test_ntag424_crypto.cpp` following the pattern of `tests/test_changekey_helpers.cpp`
  - Test the extracted crypto functions using known test vectors / round-trip verification:
    - **Encrypt/Decrypt round-trip**: Encrypt a known plaintext with a known key, then decrypt — verify output matches original
    - **Padding**: Verify `ntag424_addpadding()` adds correct padding (0x80 followed by zeros)
    - **CMAC**: Compute CMAC of a known message with a known key, compare against expected
    - **rotl**: Rotate-left a known buffer, verify output matches expected
    - **MAC**: Compute MAC with a known key and command, verify output length and non-zero
  - Use simple `assert()` or manual if/printf checks (no test framework — match existing pattern)
  - If `ntag424_random()` uses `esp_random()` (ESP32-only), provide a stub in `tests/stubs/` that calls `rand()` instead, or skip testing it with a comment
  - If `ntag424_crc32()` uses `Arduino_CRC32` class, the stub provides it

  **Deliverables for this task**:
  - `tests/stubs/Arduino.h` — minimal Arduino type/function stubs
  - `tests/stubs/Arduino_CRC32.h` — minimal CRC32 stub (or header redirect)
  - `tests/stubs/mbedtls/aes.h` — only if system mbedtls unavailable
  - `tests/test_ntag424_crypto.cpp` — the actual test file

  **Must NOT do**:
  - Do not install a test framework — plain g++ compilation
  - Do not test functions that require hardware (NFC reader)
  - Do not modify the crypto module to make it testable — if something can't be tested on desktop, skip it

  **Recommended Agent Profile**:
  - **Category**: `unspecified-high`
    - Reason: Requires understanding crypto function contracts to write meaningful tests, but not architecturally complex
  - **Skills**: []

  **Parallelization**:
  - **Can Run In Parallel**: YES (in Wave 1, but depends on Task 1 completing first)
  - **Parallel Group**: Wave 1 (starts after Task 1 delivers crypto files)
  - **Blocks**: None
  - **Blocked By**: Task 1

  **References**:

  **Pattern References**:
  - `tests/test_changekey_helpers.cpp` — Follow this exact pattern for desktop test structure, compilation approach, and assertion style

  **API/Type References**:
  - `ntag424_crypto.h` (created in Task 1) — Function signatures to test against

  **Implementation References**:
  - `Adafruit_PN532_NTAG424.cpp:1354-2067` — Original crypto implementations to understand expected behavior
  - `aescmac.h` / `aescmac.cpp` — Dependency of crypto module, needed for CMAC tests

  **External References**:
  - RFC 4493 (AES-CMAC) — Reference test vectors for CMAC verification

  **WHY Each Reference Matters**:
  - `test_changekey_helpers.cpp` is the ONLY existing test pattern — match it exactly for consistency
  - The original implementations define expected behavior — read them to derive test vectors

  **Acceptance Criteria**:

  - [ ] `tests/stubs/Arduino.h` exists with minimal type stubs
  - [ ] `tests/stubs/Arduino_CRC32.h` exists with CRC32 stub
  - [ ] `tests/test_ntag424_crypto.cpp` exists
  - [ ] Compiles: `g++ -std=c++17 -Itests/stubs -I. -o test_crypto tests/test_ntag424_crypto.cpp ntag424_crypto.cpp aescmac.cpp -lmbedcrypto` (or with mbedtls stub if system lib unavailable)
  - [ ] Runs: `./test_crypto` exits with code 0
  - [ ] Tests at least: encrypt/decrypt round-trip, padding, rotl
  - [ ] Untestable functions (if any remain after stubs) documented with skip comment

  **QA Scenarios (MANDATORY):**

  ```
  Scenario: Crypto tests compile and pass with desktop stubs
    Tool: Bash
    Preconditions: ntag424_crypto.h/.cpp exist (Task 1 complete), aescmac.h/.cpp exist, tests/stubs/ contains Arduino.h and Arduino_CRC32.h
    Steps:
      1. Run: ls tests/stubs/Arduino.h tests/stubs/Arduino_CRC32.h
      2. Assert both files exist
      3. Run: g++ -std=c++17 -Itests/stubs -I. -o test_crypto tests/test_ntag424_crypto.cpp ntag424_crypto.cpp aescmac.cpp -lmbedcrypto 2>&1
      4. Assert exit code 0 (compilation succeeds)
      5. Run: ./test_crypto 2>&1
      6. Assert exit code 0 (all tests pass)
      7. Assert output contains "PASS" or similar success indicators
    Expected Result: Stubs present, clean compilation, all tests pass
    Failure Indicators: Missing stubs, compilation error, non-zero exit code, assertion failure
    Evidence: .sisyphus/evidence/task-3-crypto-tests.txt

  Scenario: Encrypt/decrypt round-trip correctness
    Tool: Bash
    Preconditions: test_crypto binary exists and runs
    Steps:
      1. Run: ./test_crypto 2>&1
      2. Check output for encrypt/decrypt round-trip test specifically
      3. Verify the test encrypts known data, decrypts it, and compares with original
    Expected Result: Round-trip produces identical plaintext
    Failure Indicators: Output mismatch between original and decrypted data
    Evidence: .sisyphus/evidence/task-3-roundtrip.txt
  ```

  **Evidence to Capture:**
  - [ ] task-3-crypto-tests.txt
  - [ ] task-3-roundtrip.txt

  **Commit**: YES
  - Message: `test: add desktop stubs and tests for ntag424_crypto`
  - Files: `tests/stubs/Arduino.h`, `tests/stubs/Arduino_CRC32.h`, `tests/test_ntag424_crypto.cpp` (+ `tests/stubs/mbedtls/aes.h` if needed)
  - Pre-commit: `g++ -std=c++17 -Itests/stubs -I. -o test_crypto tests/test_ntag424_crypto.cpp ntag424_crypto.cpp aescmac.cpp -lmbedcrypto && ./test_crypto`

- [x] 4. Extract `ntag424_apdu.h/.cpp` from monolith

  **What to do**:
  - Create `ntag424_apdu.h` and `ntag424_apdu.cpp` in project root
  - This module contains the **reader-agnostic** parts of the current `ntag424_apdu_send()` function (lines 1384-1670 of `Adafruit_PN532_NTAG424.cpp`)
  - The current function mixes APDU construction with PN532 transport. Split it into:

  **Part A — APDU Construction** (goes to `ntag424_apdu`):
  Build the raw ISO 7816-4 APDU bytes (CLA/INS/P1/P2/Lc/data/Le) WITHOUT the PN532 framing bytes. Currently lines 1397-1520 build the `apdu[]` buffer starting at index 2 (skipping bytes 0-1 which are PN532-specific). The new function builds the APDU starting at index 0 (no PN532 prefix).

  Specifically:
  - `ntag424_build_apdu()` — takes (cla, ins, p1, p2, cmd_header, cmd_data, le, comm_mode, session, TI_buffer, apdu_out) and returns apdu_length
  - Handles all 3 comm modes: PLAIN (lines 1413-1415), MAC (lines 1416-1428), FULL (lines 1430-1516)
  - Calls crypto functions from `ntag424_crypto.h` for MAC/ENC operations
  - Does NOT call `sendCommandCheckAck()` or `readdata()` — those are reader-specific
  - The Le byte logic (lines 1517-1520) is part of this function

  **Part B — Response Processing** (goes to `ntag424_apdu`):
  - `ntag424_process_response()` — takes (response, response_length, comm_mode, session, TI_buffer, processed_response) and returns processed_response_length
  - Response MAC verification (lines 1560-1613)
  - Response decryption in FULL mode (lines 1617-1665)
  - Command counter increment (line 1668)
  - Does NOT call `readdata()` or access `pn532_packetbuffer` — receives raw response bytes

  **Part C — Helper functions** (goes to `ntag424_apdu`):
  - Move the static helpers from top of .cpp (lines 99-135):
    - `ntag424_read_simple_full_response()` — BUT change signature: instead of taking `Adafruit_PN532*`, take `NTAG424_Reader*` from the reader interface and call `reader->transceive()` + `ntag424_build_apdu()` + `ntag424_process_response()`
    - `ntag424_iso_select_file()` — same adapter: take `NTAG424_Reader*`
    - `ntag424_plain_status_ok()` — pure helper, move as-is
  - Also move these response-parsing helpers from `ntag424_changekey_utils.h` (lines 25-55) — they are NOT in the .cpp file:
    - `ntag424_response_has_status()`
    - `ntag424_plain_command_succeeded()`
    - `ntag424_copy_response_data_if_status()`

  **Include dependencies**: `#include "ntag424_crypto.h"`, `#include "ntag424_reader.h"`, `#include <string.h>`

  **Key insight**: After this extraction, the PN532-specific adapter (Task 7) will implement `transceive()` to do the InDataExchange framing (prepend 0x40, 0x01) + `sendCommandCheckAck()` + `readdata()` + response extraction from `pn532_packetbuffer`. The APDU module just builds/parses the ISO 7816 bytes.

  **Must NOT do**:
  - Do not include any PN532-specific headers or constants
  - Do not reference `pn532_packetbuffer`, `sendCommandCheckAck`, or `readdata`
  - Do not change the APDU byte sequences — identical bytes must be produced
  - Do not change response processing logic — identical verification and decryption

  **Recommended Agent Profile**:
  - **Category**: `deep`
    - Reason: This is the most architecturally complex extraction — splitting a 286-line function at the transport boundary while preserving byte-for-byte identical APDU output. Requires careful understanding of which parts are reader-agnostic vs reader-specific.
  - **Skills**: []

  **Parallelization**:
  - **Can Run In Parallel**: NO (depends on Task 1 completing)
  - **Parallel Group**: Wave 2 (with Task 5)
  - **Blocks**: Tasks 5, 6, 8
  - **Blocked By**: Task 1 (needs crypto functions)

  **References**:

  **Pattern References**:
  - `ntag424_crypto.h/.cpp` (created in Task 1) — Follow the same free-function pattern

  **API/Type References**:
  - `ntag424_reader.h` (created in Task 2) — `NTAG424_Reader::transceive()` signature for the helper functions that need to send APDUs
  - `ntag424_crypto.h` — Session struct and crypto function signatures

  **Implementation References** (CRITICAL — the exact code to split):
  - `Adafruit_PN532_NTAG424.cpp:1384-1670` — The complete `ntag424_apdu_send()` function. Lines 1397-1520 = APDU construction (reader-agnostic). Lines 1528-1554 = PN532 transport (reader-specific → goes to adapter). Lines 1560-1665 = response processing (reader-agnostic).
  - `Adafruit_PN532_NTAG424.cpp:1401-1402` — PN532 framing: `apdu[0] = PN532_COMMAND_INDATAEXCHANGE; apdu[1] = 0x01;` — these 2 bytes must NOT appear in the APDU module
  - `Adafruit_PN532_NTAG424.cpp:1528` — `sendCommandCheckAck()` call — reader-specific, stays out of APDU module
  - `Adafruit_PN532_NTAG424.cpp:1543` — `readdata(pn532_packetbuffer, ...)` — reader-specific
  - `Adafruit_PN532_NTAG424.cpp:1553-1554` — Response extraction from `pn532_packetbuffer` — reader-specific
  - `Adafruit_PN532_NTAG424.cpp:99-135` — Static helper functions to move
  - `ntag424_changekey_utils.h:25-55` — Response parsing helpers (`ntag424_response_has_status`, `ntag424_plain_command_succeeded`, `ntag424_copy_response_data_if_status`) are defined here as `static inline` functions. NOT in `Adafruit_PN532_NTAG424.cpp`.

  **WHY Each Reference Matters**:
  - Lines 1401-1402 are THE split boundary — everything before index 2 in the apdu[] array is PN532-specific, everything from index 2 onward is ISO 7816
  - Lines 1528-1554 are the other split boundary — sending and receiving via PN532 transport
  - The static helpers (99-135) currently take `Adafruit_PN532*` and call `ntag424_apdu_send` — they need signature changes to use the reader interface instead

  **Acceptance Criteria**:

  - [ ] `ntag424_apdu.h` exists in project root with include guard
  - [ ] `ntag424_apdu.cpp` exists in project root
  - [ ] `ntag424_build_apdu()` function handles PLAIN, MAC, and FULL comm modes
  - [ ] `ntag424_process_response()` function handles response MAC verification and decryption
  - [ ] No references to `PN532_COMMAND_INDATAEXCHANGE`, `sendCommandCheckAck`, `readdata`, or `pn532_packetbuffer`
  - [ ] Helper functions (`ntag424_read_simple_full_response`, etc.) use `NTAG424_Reader*` instead of `Adafruit_PN532*`
  - [ ] `#include "ntag424_apdu.h"` compiles without errors

  **QA Scenarios (MANDATORY):**

  ```
  Scenario: APDU module has no PN532 coupling
    Tool: Bash
    Preconditions: ntag424_apdu.h and ntag424_apdu.cpp exist
    Steps:
      1. Run: grep -c 'PN532\|pn532\|Adafruit\|sendCommandCheckAck\|readdata\|packetbuffer' ntag424_apdu.h ntag424_apdu.cpp
      2. Assert total count = 0 (zero PN532 references)
      3. Run: grep -c 'ntag424_build_apdu\|ntag424_process_response' ntag424_apdu.h
      4. Assert count >= 2 (both core functions declared)
    Expected Result: Zero PN532 coupling, both core functions present
    Failure Indicators: Any PN532/Adafruit reference found, missing core functions
    Evidence: .sisyphus/evidence/task-4-no-pn532-coupling.txt

  Scenario: APDU module includes crypto and reader interface
    Tool: Bash
    Preconditions: ntag424_apdu.h exists
    Steps:
      1. Run: grep '#include.*ntag424_crypto' ntag424_apdu.h
      2. Assert match found (depends on crypto module)
      3. Run: grep '#include.*ntag424_reader' ntag424_apdu.h
      4. Assert match found (depends on reader interface)
    Expected Result: Both dependencies properly included
    Failure Indicators: Missing include for crypto or reader
    Evidence: .sisyphus/evidence/task-4-includes.txt

  Scenario: Helper functions use reader interface
    Tool: Bash
    Preconditions: ntag424_apdu.h/.cpp exist
    Steps:
      1. Run: grep 'NTAG424_Reader' ntag424_apdu.h ntag424_apdu.cpp
      2. Assert matches found (helper functions take NTAG424_Reader* parameter)
      3. Run: grep 'Adafruit_PN532\s*\*' ntag424_apdu.h ntag424_apdu.cpp
      4. Assert count = 0 (no old-style Adafruit_PN532* parameters)
    Expected Result: Helpers use NTAG424_Reader*, not Adafruit_PN532*
    Failure Indicators: Old Adafruit_PN532* signatures still present
    Evidence: .sisyphus/evidence/task-4-reader-interface-usage.txt
  ```

  **Evidence to Capture:**
  - [ ] task-4-no-pn532-coupling.txt
  - [ ] task-4-includes.txt
  - [ ] task-4-reader-interface-usage.txt

  **Commit**: YES
  - Message: `refactor: extract ntag424_apdu.h/.cpp from monolith`
  - Files: `ntag424_apdu.h`, `ntag424_apdu.cpp`
  - Pre-commit: syntax check passes

- [x] 5. Desktop tests for ntag424_apdu module

  **What to do**:
  - Create `tests/test_ntag424_apdu.cpp` following the pattern of `tests/test_changekey_helpers.cpp`
  - Test the APDU construction and response processing functions:
    - **PLAIN mode APDU**: Call `ntag424_build_apdu()` with PLAIN comm mode, known CLA/INS/P1/P2/data, verify output bytes match expected ISO 7816-4 APDU format (CLA + INS + P1 + P2 + Lc + data + Le)
    - **Response parsing helpers**: Test `ntag424_response_has_status()`, `ntag424_plain_command_succeeded()`, `ntag424_copy_response_data_if_status()` with known response buffers
    - **MAC mode APDU**: Requires session state — test with a known session and verify the APDU includes 8-byte MAC at the end
    - **Response MAC verification**: Construct a response with known MAC, verify `ntag424_process_response()` accepts valid MAC and rejects tampered MAC
  - Create a mock `NTAG424_Reader` implementation for testing that records calls and returns canned responses
  - Compile on desktop (not Arduino). Mock Arduino types as needed with minimal stubs.
  - If some tests require ESP32-specific features, skip them with a comment

  **Must NOT do**:
  - Do not install a test framework
  - Do not test PN532-specific behavior — APDU module is reader-agnostic
  - Do not modify the APDU module to make it testable

  **Recommended Agent Profile**:
  - **Category**: `unspecified-high`
    - Reason: Requires understanding APDU construction contracts and creating mock reader, but not architecturally complex
  - **Skills**: []

  **Parallelization**:
  - **Can Run In Parallel**: YES (in Wave 2, after Task 4)
  - **Parallel Group**: Wave 2 (starts after Task 4 delivers APDU files)
  - **Blocks**: None
  - **Blocked By**: Task 4

  **References**:

  **Pattern References**:
  - `tests/test_changekey_helpers.cpp` — Desktop test pattern to follow
  - `tests/test_ntag424_crypto.cpp` (created in Task 3) — Recent test file for consistency

  **API/Type References**:
  - `ntag424_apdu.h` (created in Task 4) — Function signatures to test
  - `ntag424_reader.h` (created in Task 2) — Interface to mock for testing

  **Implementation References**:
  - `Adafruit_PN532_NTAG424.cpp:1397-1520` — Original APDU construction logic to derive test vectors from (known inputs → expected APDU bytes)

  **WHY Each Reference Matters**:
  - The original implementation (lines 1397-1520) defines expected byte sequences — use it to create known-answer test vectors
  - The reader interface needs to be mocked to test helper functions that call `transceive()`

  **Acceptance Criteria**:

  - [ ] `tests/test_ntag424_apdu.cpp` exists
  - [ ] Compiles: `g++ -std=c++17 -Itests/stubs -I. -o test_apdu tests/test_ntag424_apdu.cpp ntag424_apdu.cpp ntag424_crypto.cpp aescmac.cpp -lmbedcrypto`
  - [ ] Runs: `./test_apdu` exits with code 0
  - [ ] Tests at least: PLAIN mode APDU construction, response parsing helpers
  - [ ] Contains mock NTAG424_Reader implementation

  **QA Scenarios (MANDATORY):**

  ```
  Scenario: APDU tests compile and pass
    Tool: Bash
    Preconditions: ntag424_apdu.h/.cpp, ntag424_crypto.h/.cpp, ntag424_reader.h, aescmac.h/.cpp all exist
    Steps:
      1. Run: g++ -std=c++17 -Itests/stubs -I. -o test_apdu tests/test_ntag424_apdu.cpp ntag424_apdu.cpp ntag424_crypto.cpp aescmac.cpp -lmbedcrypto 2>&1
      2. Assert exit code 0
      3. Run: ./test_apdu 2>&1
      4. Assert exit code 0
      5. Assert output contains test pass indicators
    Expected Result: Clean compilation and all tests pass
    Failure Indicators: Compilation error, non-zero exit code, assertion failure
    Evidence: .sisyphus/evidence/task-5-apdu-tests.txt

  Scenario: PLAIN mode APDU construction produces correct bytes
    Tool: Bash
    Preconditions: test_apdu binary exists
    Steps:
      1. Run: ./test_apdu 2>&1
      2. Verify output shows PLAIN mode test with known CLA=0x90, INS, P1=0, P2=0
      3. Verify constructed APDU starts with 0x90 (CLA) followed by INS byte
    Expected Result: APDU bytes match ISO 7816-4 format exactly
    Failure Indicators: Wrong byte order or missing APDU fields
    Evidence: .sisyphus/evidence/task-5-plain-apdu.txt
  ```

  **Evidence to Capture:**
  - [ ] task-5-apdu-tests.txt
  - [ ] task-5-plain-apdu.txt

  **Commit**: YES
  - Message: `test: add desktop tests for ntag424_apdu`
  - Files: `tests/test_ntag424_apdu.cpp`
  - Pre-commit: `g++ -std=c++17 -Itests/stubs -I. -o test_apdu tests/test_ntag424_apdu.cpp ntag424_apdu.cpp ntag424_crypto.cpp aescmac.cpp -lmbedcrypto && ./test_apdu`

- [ ] 6. Extract `ntag424_core.h/.cpp` with session structs and protocol logic

  **What to do**:
  - Create `ntag424_core.h` and `ntag424_core.cpp` in project root
  - This module contains NTAG424 protocol-level command orchestration — the functions that build specific NTAG424 commands using the APDU layer
  - **Move struct definitions** from `Adafruit_PN532_NTAG424.h` to `ntag424_core.h`:
    - `ntag424_VersionInfoType` (lines 316-336)
    - `ntag424_FileSettings` (lines 340-357)
    - Auth response size/offset constants (lines 278-292)
    - `ntag424_SessionType` — currently in `ntag424_crypto.h` from Task 1. Move it here and have `ntag424_crypto.h` include `ntag424_core.h` instead (or keep it in crypto.h and include from core — choose whichever avoids circular dependencies. Preferred: keep `ntag424_SessionType` in a shared header `ntag424_types.h` included by both, OR keep it in `ntag424_crypto.h` since crypto was extracted first)
  - **Move NTAG424 command constants** from `Adafruit_PN532_NTAG424.h` to `ntag424_core.h`:
    - All `NTAG424_*` defines (lines 86-106): comm modes, CLA, command codes, ISO commands
  - **Move protocol functions** as free functions taking `NTAG424_Reader*` and session state:
    - `ntag424_GetFileSettings(reader, session, fileno, buffer, comm_mode)` — currently a class method calling `ntag424_apdu_send`
    - `ntag424_ChangeFileSettings(reader, session, fileno, filesettings, length, comm_mode)`
    - `ntag424_ChangeKey(reader, session, oldkey, newkey, keynumber, keyversion)`
    - `ntag424_GetCardUID(reader, session, buffer)`
    - `ntag424_GetTTStatus(reader, session, buffer)`
    - `ntag424_GetKeyVersion(reader, session, keyno, version)`
    - `ntag424_ReadSig(reader, session, buffer)`
    - `ntag424_ISOSelectFileById(reader, fileid)`
    - `ntag424_ISOSelectFileByDFN(reader, dfn)`
    - `ntag424_ISOSelectNDEFFile(reader)`
    - `ntag424_ISOSelectCCFile(reader)`
    - `ntag424_FormatNDEF(reader, session)`
    - `ntag424_ISOUpdateBinary(reader, session, buffer, length)`
    - `ntag424_ReadNDEFMessage(reader, session, buffer, maxsize)`
    - `ntag424_isNTAG424(reader)` — calls GetVersion internally
    - `ntag424_GetVersion(reader, versioninfo)` — the 3-frame exchange (lines 2663-2800), currently tightly coupled to PN532. Refactor to use multiple `reader->transceive()` calls
    - `ntag424_Authenticate(reader, session, TI, authresponse_buffers, key, keyno, cmd)` — the 3-step auth (lines 2105-2352), refactor to use `reader->transceive()` calls
    - `ntag424_ReadData(reader, session, buffer, fileno, offset, size)` — lines 2561-2636, refactor to use `reader->transceive()`
    - `ntag424_ISOReadFile(reader, session, buffer, maxsize)` — lines 2998-3205, refactor
    - `ntag424_ISOReadBinary(reader, session, offset, le, response, bufsize)` — lines 3225-3282, refactor
  - The 5 tightly-coupled methods (Authenticate, GetVersion, ReadData, ISOReadFile, ISOReadBinary) are the hardest part. They currently use `pn532_packetbuffer`, `sendCommandCheckAck()`, and `readdata()` directly. They must be rewritten to use `reader->transceive()` instead. The key transformation:
    - Old pattern: build `pn532_packetbuffer` with InDataExchange command → `sendCommandCheckAck()` → `readdata(pn532_packetbuffer)` → extract response from buffer
    - New pattern: build raw APDU bytes (without InDataExchange prefix) → `reader->transceive(apdu, len, response, maxlen)` → process response directly
  - **Include dependencies**: `#include "ntag424_crypto.h"`, `#include "ntag424_apdu.h"`, `#include "ntag424_reader.h"`

  **Must NOT do**:
  - Do not include PN532-specific headers or use PN532 constants (no `PN532_COMMAND_INDATAEXCHANGE`)
  - Do not reference `pn532_packetbuffer`, `sendCommandCheckAck`, `readdata`
  - Do not change protocol behavior — identical NTAG424 command sequences
  - Do not add new error handling beyond what exists

  **Recommended Agent Profile**:
  - **Category**: `deep`
    - Reason: Contains the hardest extractions — converting 5 tightly-coupled methods that directly manipulate PN532 packet buffers into reader-agnostic code using `transceive()`. Each method needs careful line-by-line analysis.
  - **Skills**: []

  **Parallelization**:
  - **Can Run In Parallel**: YES (with Task 7 in Wave 3)
  - **Parallel Group**: Wave 3 (with Task 7)
  - **Blocks**: Task 8
  - **Blocked By**: Tasks 1, 2, 4

  **References**:

  **Pattern References**:
  - `ntag424_crypto.h/.cpp` (Task 1) — Free function pattern with session pointer
  - `ntag424_apdu.h/.cpp` (Task 4) — APDU construction/response processing functions to call

  **API/Type References**:
  - `ntag424_reader.h` (Task 2) — `NTAG424_Reader::transceive()` signature to call
  - `Adafruit_PN532_NTAG424.h:219-276` — All NTAG424 method signatures (the public API to replicate as free functions)
  - `Adafruit_PN532_NTAG424.h:278-357` — All struct/constant definitions to move

  **Implementation References** (the tightly-coupled methods to rewrite):
  - `Adafruit_PN532_NTAG424.cpp:2105-2352` — `ntag424_Authenticate()` — 3-step auth with direct PN532 framing
  - `Adafruit_PN532_NTAG424.cpp:2663-2800` — `ntag424_GetVersion()` — 3-frame exchange with InDataExchange
  - `Adafruit_PN532_NTAG424.cpp:2561-2636` — `ntag424_ReadData()` — direct packet buffer
  - `Adafruit_PN532_NTAG424.cpp:2998-3205` — `ntag424_ISOReadFile()` — direct packet buffer
  - `Adafruit_PN532_NTAG424.cpp:3225-3282` — `ntag424_ISOReadBinary()` — direct packet buffer

  **Implementation References** (cleanly-delegated methods to move with minimal changes):
  - `Adafruit_PN532_NTAG424.cpp` — Search for each method: `grep -n 'Adafruit_PN532::ntag424_Get\|Adafruit_PN532::ntag424_Change\|Adafruit_PN532::ntag424_ISO\|Adafruit_PN532::ntag424_Format\|Adafruit_PN532::ntag424_Read'`
  - These methods call `ntag424_apdu_send()` — they'll now call `ntag424_build_apdu()` + `reader->transceive()` + `ntag424_process_response()` (or a convenience wrapper combining these)

  **WHY Each Reference Matters**:
  - The 5 tightly-coupled methods (Authenticate, GetVersion, ReadData, ISOReadFile, ISOReadBinary) are the hardest part — each needs careful understanding of which bytes are PN532 framing vs NTAG424 protocol
  - The struct definitions must be moved verbatim — sketches access `nfc.ntag424_Session`, `nfc.ntag424_VersionInfo` etc. through the wrapper class
  - The method signatures (lines 219-276) define the exact contracts the free functions must replicate

  **Acceptance Criteria**:

  - [ ] `ntag424_core.h` exists with all NTAG424 struct definitions and command constants
  - [ ] `ntag424_core.cpp` exists with all NTAG424 protocol functions as free functions
  - [ ] All 5 tightly-coupled methods converted to use `reader->transceive()` instead of PN532 direct access
  - [ ] All 13+ cleanly-delegated methods converted to free functions with reader parameter
  - [ ] Zero references to `PN532`, `pn532`, `Adafruit`, `sendCommandCheckAck`, `readdata`, `packetbuffer`
  - [ ] Include chain: `ntag424_core.h` includes `ntag424_crypto.h`, `ntag424_apdu.h`, `ntag424_reader.h`

  **QA Scenarios (MANDATORY):**

  ```
  Scenario: Core module has no PN532 coupling
    Tool: Bash
    Preconditions: ntag424_core.h and ntag424_core.cpp exist
    Steps:
      1. Run: grep -c 'PN532\|pn532\|Adafruit\|sendCommandCheckAck\|readdata\|packetbuffer' ntag424_core.h ntag424_core.cpp
      2. Assert total count = 0
      3. Run: grep -c 'NTAG424_Reader' ntag424_core.h ntag424_core.cpp
      4. Assert count >= 5 (reader interface used throughout)
    Expected Result: Zero PN532 references, reader interface used for all transport
    Failure Indicators: Any PN532 reference found, or missing reader interface usage
    Evidence: .sisyphus/evidence/task-6-no-pn532.txt

  Scenario: All NTAG424 structs are defined
    Tool: Bash
    Preconditions: ntag424_core.h exists
    Steps:
      1. Run: grep 'struct ntag424_VersionInfoType' ntag424_core.h
      2. Assert match found
      3. Run: grep 'struct ntag424_FileSettings' ntag424_core.h
      4. Assert match found
      5. Run: grep 'NTAG424_COM_CLA\|NTAG424_CMD_GETVERSION\|NTAG424_COMM_MODE_PLAIN' ntag424_core.h
      6. Assert all 3 found (command constants moved)
    Expected Result: All structs and command constants present in core header
    Failure Indicators: Missing struct or constant definitions
    Evidence: .sisyphus/evidence/task-6-structs.txt

  Scenario: Tightly-coupled methods use reader->transceive
    Tool: Bash
    Preconditions: ntag424_core.cpp exists
    Steps:
      1. Run: grep -A5 'ntag424_Authenticate' ntag424_core.cpp | head -20
      2. Verify function takes NTAG424_Reader* parameter
      3. Run: grep 'transceive' ntag424_core.cpp
      4. Assert multiple matches (used in Authenticate, GetVersion, ReadData, ISOReadFile, ISOReadBinary)
      5. Run: grep -c 'transceive' ntag424_core.cpp
      6. Assert count >= 5
    Expected Result: All tightly-coupled methods call reader->transceive()
    Failure Indicators: Missing transceive calls, or direct buffer manipulation
    Evidence: .sisyphus/evidence/task-6-transceive-usage.txt
  ```

  **Evidence to Capture:**
  - [ ] task-6-no-pn532.txt
  - [ ] task-6-structs.txt
  - [ ] task-6-transceive-usage.txt

  **Commit**: YES
  - Message: `refactor: extract ntag424_core.h/.cpp with session structs`
  - Files: `ntag424_core.h`, `ntag424_core.cpp`
  - Pre-commit: syntax check passes

- [ ] 7. Create `pn532_ntag424_adapter.h/.cpp`

  **What to do**:
  - Create `pn532_ntag424_adapter.h` and `pn532_ntag424_adapter.cpp` in project root
  - This class implements `NTAG424_Reader` for the PN532 hardware
  - Class definition:
    ```cpp
    class PN532_NTAG424_Adapter : public NTAG424_Reader {
    public:
      // Takes a pointer to the existing Adafruit_PN532 instance
      // (the adapter doesn't own the PN532 — the wrapper class does)
      explicit PN532_NTAG424_Adapter(Adafruit_PN532 *pn532);

      uint8_t transceive(const uint8_t *send, uint8_t sendLength,
                         uint8_t *response, uint8_t responseMaxLength) override;
      uint8_t get_uid(uint8_t *uid, uint8_t uidMaxLength) override;
      bool is_tag_present() override;

    private:
      Adafruit_PN532 *_pn532;
    };
    ```
  - **`transceive()` implementation** — wraps the ISO 7816 APDU in PN532 InDataExchange framing:
    1. Allocate buffer: `uint8_t pn532_frame[sendLength + 2]`
    2. Prepend: `pn532_frame[0] = PN532_COMMAND_INDATAEXCHANGE; pn532_frame[1] = 0x01;`
    3. Copy APDU: `memcpy(pn532_frame + 2, send, sendLength)`
    4. Call: `_pn532->sendCommandCheckAck(pn532_frame, sendLength + 2)`
    5. Read response: `_pn532->readdata(response_buffer, 64)` (into local buffer)
    6. Extract response: parse response length from PN532 frame header, copy payload to `response`
    7. Return response length
    - This is exactly what lines 1401-1402 (framing) + 1528 (send) + 1543-1554 (read+extract) of `ntag424_apdu_send()` currently do
  - **`get_uid()` implementation** — returns `_pn532->_uid` and `_pn532->_uidLen`. BUT `_uid` and `_uidLen` are private members (line 371-372). Options:
    - Option A: Make them accessible via public getters (add `getUID()` method to Adafruit_PN532)
    - Option B: Make `PN532_NTAG424_Adapter` a friend class
    - Option C: Store UID in the adapter when `readPassiveTargetID()` is called
    - **Preferred**: Option A — add simple public `getUID(uint8_t *uid)` and `getUIDLength()` to Adafruit_PN532. This is a minimal, non-speculative change.
  - **`is_tag_present()` implementation** — returns true if `_pn532->_inListedTag > 0` (tag has been listed). Same access pattern as UID — add public getter if needed.
  - **Include dependencies for `pn532_ntag424_adapter.h`**: `#include "ntag424_reader.h"` and a **forward declaration** `class Adafruit_PN532;` — do NOT include `Adafruit_PN532_NTAG424.h` in the header to avoid circular includes.
  - **Include dependencies for `pn532_ntag424_adapter.cpp`**: `#include "pn532_ntag424_adapter.h"` and `#include "Adafruit_PN532_NTAG424.h"` (the full header is needed only in the .cpp where methods are implemented).
  - Note: This avoids a header cycle: `Adafruit_PN532_NTAG424.h` includes `pn532_ntag424_adapter.h`, which forward-declares `Adafruit_PN532` without including back. The full PN532 class is available in `pn532_ntag424_adapter.cpp` via its own include.

  **Must NOT do**:
  - Do not add methods beyond the 3 required by the interface
  - Do not add `configure()`, `reset()`, or `get_firmware_version()` to the adapter
  - Do not change PN532 transport logic — just wrap existing methods
  - Do not own or manage the PN532 instance lifecycle (just hold a pointer)

  **Recommended Agent Profile**:
  - **Category**: `deep`
    - Reason: Requires understanding PN532 transport framing to implement transceive() correctly, and handling private member access for UID
  - **Skills**: []

  **Parallelization**:
  - **Can Run In Parallel**: YES (with Task 6 in Wave 3)
  - **Parallel Group**: Wave 3 (with Task 6)
  - **Blocks**: Task 8
  - **Blocked By**: Task 2 (needs reader interface)

  **References**:

  **Pattern References**:
  - `Adafruit_PN532_NTAG424.cpp:1401-1402, 1528-1554` — The exact PN532 framing code to replicate in `transceive()`

  **API/Type References**:
  - `ntag424_reader.h` (Task 2) — Interface to implement
  - `Adafruit_PN532_NTAG424.h:184-185` — `sendCommandCheckAck()` signature
  - `Adafruit_PN532_NTAG424.h:377` — `readdata()` signature (private — adapter needs friend or public wrapper)
  - `Adafruit_PN532_NTAG424.h:369-374` — Private members `_uid`, `_uidLen`, `_inListedTag`

  **Implementation References**:
  - `Adafruit_PN532_NTAG424.cpp:1401-1402` — PN532 framing: `apdu[0] = PN532_COMMAND_INDATAEXCHANGE; apdu[1] = 0x01;`
  - `Adafruit_PN532_NTAG424.cpp:1528` — `sendCommandCheckAck((uint8_t *)apdu, apdusize)` call
  - `Adafruit_PN532_NTAG424.cpp:1543` — `readdata(pn532_packetbuffer, sizeof(pn532_packetbuffer))` call
  - `Adafruit_PN532_NTAG424.cpp:1553-1554` — Response extraction: `response_length = pn532_packetbuffer[3] - 3; memcpy(response, pn532_packetbuffer + 8, response_length)`
  - `Adafruit_PN532_NTAG424.cpp:96` — `pn532_packetbuffer[64]` global — adapter will use its own local buffer

  **WHY Each Reference Matters**:
  - Lines 1401-1402 define the EXACT framing bytes the adapter must prepend
  - Lines 1528, 1543, 1553-1554 define the EXACT transport sequence to replicate
  - The private member access (lines 369-374) determines whether we need friend declarations or public getters

  **Acceptance Criteria**:

  - [ ] `pn532_ntag424_adapter.h` exists with `PN532_NTAG424_Adapter` class declaration
  - [ ] `pn532_ntag424_adapter.cpp` exists with implementations
  - [ ] Class inherits from `NTAG424_Reader` and implements all 3 methods
  - [ ] `transceive()` prepends InDataExchange framing, calls `sendCommandCheckAck`/`readdata`
  - [ ] `get_uid()` returns tag UID via public getter or friend access
  - [ ] `is_tag_present()` returns tag presence status
  - [ ] Uses its own local buffer (not the global `pn532_packetbuffer`)

  **QA Scenarios (MANDATORY):**

  ```
  Scenario: Adapter implements reader interface correctly
    Tool: Bash
    Preconditions: pn532_ntag424_adapter.h exists
    Steps:
      1. Run: grep 'class PN532_NTAG424_Adapter.*:.*public NTAG424_Reader' pn532_ntag424_adapter.h
      2. Assert match found (inherits from NTAG424_Reader)
      3. Run: grep -c 'override' pn532_ntag424_adapter.h
      4. Assert count = 3 (all 3 methods overridden)
      5. Run: grep 'PN532_COMMAND_INDATAEXCHANGE' pn532_ntag424_adapter.cpp
      6. Assert match found (framing byte used in transceive)
    Expected Result: Correct inheritance, all 3 overrides, InDataExchange framing present
    Failure Indicators: Missing inheritance, wrong override count, missing framing
    Evidence: .sisyphus/evidence/task-7-adapter-structure.txt

  Scenario: Adapter uses local buffer, not global
    Tool: Bash
    Preconditions: pn532_ntag424_adapter.cpp exists
    Steps:
      1. Run: grep 'pn532_packetbuffer' pn532_ntag424_adapter.cpp
      2. Assert count = 0 (does not use global buffer)
      3. Run: grep 'uint8_t.*\[64\]\|uint8_t.*buf' pn532_ntag424_adapter.cpp
      4. Assert match found (uses local buffer)
    Expected Result: Local buffer used, global buffer not referenced
    Failure Indicators: Reference to pn532_packetbuffer found
    Evidence: .sisyphus/evidence/task-7-local-buffer.txt
  ```

  **Evidence to Capture:**
  - [ ] task-7-adapter-structure.txt
  - [ ] task-7-local-buffer.txt

  **Commit**: YES
  - Message: `refactor: add pn532_ntag424_adapter.h/.cpp`
  - Files: `pn532_ntag424_adapter.h`, `pn532_ntag424_adapter.cpp`
  - Pre-commit: syntax check passes

- [x] 8. Convert `Adafruit_PN532_NTAG424` to thin compatibility wrapper

  **What to do**:
  This is the integration task — modify `Adafruit_PN532_NTAG424.h/.cpp` so that:
  1. The class still exists with identical public API (all 66 public methods have same signatures)
  2. NTAG424 methods delegate to the extracted free functions in `ntag424_core`
  3. The class owns a `PN532_NTAG424_Adapter` instance (or creates one on demand)
  4. Session state, version info, and auth response buffers remain as public class members (sketches access them directly: `nfc.ntag424_Session`, `nfc.ntag424_VersionInfo`, etc.)
  5. Non-NTAG424 methods (PN532 base, Mifare, NTAG2xx) remain unchanged in the .cpp

  **Specific changes to `Adafruit_PN532_NTAG424.h`**:
  - Add `#include "ntag424_core.h"` (which transitively includes crypto, apdu, reader)
  - Add `#include "pn532_ntag424_adapter.h"`
  - Remove NTAG424 command constants (now in `ntag424_core.h`) — but keep `#include "ntag424_core.h"` so they're still visible
  - Remove struct definitions that moved to `ntag424_core.h` — but keep the member variable declarations (`ntag424_Session`, `ntag424_VersionInfo`, etc.) using the types from `ntag424_core.h`
  - **Keep ALL crypto method declarations** — they become thin wrappers that delegate to the free functions in `ntag424_crypto.h`. This preserves the public API. Example: `uint8_t ntag424_crc32(uint8_t *data, uint8_t datalength)` stays declared but its implementation becomes `{ return ::ntag424_crc32(data, datalength); }`
  - Keep NTAG424 command method declarations BUT their implementations now delegate
  - Add private member: `PN532_NTAG424_Adapter _ntag424_adapter;` (initialized in constructors with `this`)
  - If public getters were added for UID/tag access (Task 7), add those declarations here

  **Specific changes to `Adafruit_PN532_NTAG424.cpp`**:
  - Replace all crypto function implementations with thin delegation wrappers. Example:
    ```cpp
    uint32_t Adafruit_PN532::ntag424_crc32(uint8_t *data, uint8_t datalength) {
      return ::ntag424_crc32(data, datalength);
    }
    ```
  - For crypto methods that previously accessed `this->ntag424_Session`, pass `&ntag424_Session`:
    ```cpp
    uint8_t Adafruit_PN532::ntag424_MAC(uint8_t *cmd, uint8_t *cmdheader, uint8_t cmdheader_length,
                                         uint8_t *cmddata, uint8_t cmddata_length, uint8_t *signature) {
      return ::ntag424_MAC(&ntag424_Session, cmd, cmdheader, cmdheader_length, cmddata, cmddata_length, signature);
    }
    ```
  - Remove the old `ntag424_apdu_send()` implementation body and replace with delegation to `ntag424_build_apdu()` + `_ntag424_adapter.transceive()` + `ntag424_process_response()`
  - Remove protocol function implementation bodies and replace with delegation wrappers
  - Remove the static helper functions at top (lines 99-135, now in `ntag424_apdu.cpp`)
  - Keep ALL non-NTAG424 code (constructors, begin, PN532 commands, Mifare, NTAG2xx, transport) completely unchanged
  - Initialize `_ntag424_adapter` in each constructor: `_ntag424_adapter(this)` (adapter holds back-pointer to PN532)

  **Must NOT do**:
  - Do not change any public method signature — identical parameters and return types
  - Do not modify non-NTAG424 methods (PN532 base, Mifare, NTAG2xx, transport)
  - Do not change example sketches
  - Do not rename the class
  - Do not change `library.properties`
  - Do not remove public member variables (ntag424_Session, etc.) — sketches access them

  **Recommended Agent Profile**:
  - **Category**: `deep`
    - Reason: Large-scope integration task touching the two biggest files. Must carefully thread the needle: remove extracted code, add delegation wrappers, preserve API, initialize adapter, handle state passing. High risk of breaking things.
  - **Skills**: []

  **Parallelization**:
  - **Can Run In Parallel**: NO
  - **Parallel Group**: Wave 4 (sequential — must be after all extractions)
  - **Blocks**: Task 9
  - **Blocked By**: Tasks 4, 6, 7

  **References**:

  **Pattern References**:
  - `ntag424_core.h/.cpp` (Task 6) — Free function signatures to delegate to
  - `pn532_ntag424_adapter.h` (Task 7) — Adapter class to instantiate

  **API/Type References**:
  - `Adafruit_PN532_NTAG424.h:168-386` — Complete class definition (THE source of truth for what must be preserved)
  - `Adafruit_PN532_NTAG424.h:219-276` — All NTAG424 method signatures (must remain identical)
  - `Adafruit_PN532_NTAG424.h:294-338` — Public member variables (must remain accessible)

  **Implementation References**:
  - `Adafruit_PN532_NTAG424.cpp:137-872` — PN532 base code (DO NOT TOUCH)
  - `Adafruit_PN532_NTAG424.cpp:873-1670` — Crypto + APDU code to REMOVE (now in separate files)
  - `Adafruit_PN532_NTAG424.cpp:1672-3520` (approximate) — NTAG424 protocol methods to convert to delegation wrappers
  - `Adafruit_PN532_NTAG424.cpp:3523-3839` — PN532 transport code (DO NOT TOUCH)
  - `examples/ntag424_examples/ntag424_examples.ino` — Reference for how sketches use the API (to verify nothing breaks)
  - `examples/ntag424_isoreadfile/ntag424_isoreadfile.ino` — Another reference

  **WHY Each Reference Matters**:
  - The header (168-386) is the compatibility contract — every line must be accounted for
  - The PN532 base code (137-872, 3523-3839) must NOT be modified — this is the safety boundary
  - The example sketches show real usage patterns that must continue working
  - The protocol implementations (1672-3520) show what each delegation wrapper needs to pass through

  **Acceptance Criteria**:

  - [ ] All public method signatures in `Adafruit_PN532` class are IDENTICAL to before
  - [ ] All public member variables (`ntag424_Session`, `ntag424_VersionInfo`, auth buffers) still present
  - [ ] NTAG424 methods delegate to free functions in `ntag424_core`
  - [ ] Crypto methods delegate to free functions in `ntag424_crypto` (thin wrappers preserving public API)
  - [ ] `_ntag424_adapter` member initialized in all constructors
  - [ ] Non-NTAG424 code (PN532, Mifare, transport) is UNCHANGED
  - [ ] `#include <Adafruit_PN532_NTAG424.h>` pulls in all needed types
  - [ ] Crypto implementations in `Adafruit_PN532_NTAG424.cpp` replaced with thin delegation wrappers (not removed — public API preserved)
  - [ ] Old `ntag424_apdu_send()` replaced with compatibility wrapper
  - [ ] Static helpers removed from top of `.cpp` (now in `ntag424_apdu.cpp`)
  - [ ] Both example sketches compile: `arduino-cli compile --fqbn esp32:esp32:esp32 examples/ntag424_examples/ntag424_examples.ino`

  **QA Scenarios (MANDATORY):**

  ```
  Scenario: Public API is unchanged
    Tool: Bash
    Preconditions: Modified Adafruit_PN532_NTAG424.h exists
    Steps:
      1. Run: grep -c 'ntag424_' Adafruit_PN532_NTAG424.h
      2. Compare with pre-refactor count (should have SAME or MORE ntag424 references due to includes)
      3. Run: grep 'uint8_t ntag424_Authenticate\|uint8_t ntag424_GetVersion\|uint8_t ntag424_ReadData\|uint8_t ntag424_apdu_send' Adafruit_PN532_NTAG424.h
      4. Assert all 4 found (key methods still declared)
      5. Run: grep 'ntag424_Session\|ntag424_VersionInfo\|ntag424_authresponse_TI' Adafruit_PN532_NTAG424.h
      6. Assert all 3 found (public members still present)
    Expected Result: All public methods and members preserved
    Failure Indicators: Missing method declaration or member variable
    Evidence: .sisyphus/evidence/task-8-api-preserved.txt

  Scenario: Example sketches compile
    Tool: Bash
    Preconditions: All source files in place, arduino-cli available (or equivalent)
    Steps:
      1. Run: arduino-cli compile --fqbn esp32:esp32:esp32 examples/ntag424_examples/ntag424_examples.ino 2>&1
      2. Assert exit code 0 (or check for compilation success message)
      3. Run: arduino-cli compile --fqbn esp32:esp32:esp32 examples/ntag424_isoreadfile/ntag424_isoreadfile.ino 2>&1
      4. Assert exit code 0
      5. If arduino-cli unavailable, verify include chain: grep '#include' in each new file, trace resolution
    Expected Result: Both examples compile without errors
    Failure Indicators: Compilation error, missing include, unresolved symbol
    Evidence: .sisyphus/evidence/task-8-examples-compile.txt

  Scenario: Non-NTAG424 code is untouched
    Tool: Bash
    Preconditions: git history available
    Steps:
      1. Run: git diff Adafruit_PN532_NTAG424.cpp | grep '^[-+]' | grep -v 'ntag424\|#include\|_ntag424_adapter\|^[-+][-+][-+]\|^[-+]$'
      2. Assert minimal or zero output (only NTAG424-related lines changed)
      3. Check that constructor changes are limited to adapter initialization
    Expected Result: Only NTAG424 code modified, PN532/Mifare/transport untouched
    Failure Indicators: Changes to non-NTAG424 methods found in diff
    Evidence: .sisyphus/evidence/task-8-non-ntag-untouched.txt

  Scenario: Delegation wrappers call core functions
    Tool: Bash
    Preconditions: Modified Adafruit_PN532_NTAG424.cpp exists
    Steps:
      1. Run: grep '::ntag424_GetFileSettings\|::ntag424_ChangeKey\|::ntag424_Authenticate' Adafruit_PN532_NTAG424.cpp
      2. For each match, verify the function body contains a call to the corresponding free function (e.g., `::ntag424_GetFileSettings(` or `ntag424_GetFileSettings(&_ntag424_adapter,`)
      3. Run: grep '_ntag424_adapter' Adafruit_PN532_NTAG424.cpp
      4. Assert multiple matches (adapter used in delegation)
    Expected Result: Each NTAG424 method delegates through the adapter to core functions
    Failure Indicators: Methods still contain direct PN532 manipulation instead of delegation
    Evidence: .sisyphus/evidence/task-8-delegation.txt
  ```

  **Evidence to Capture:**
  - [ ] task-8-api-preserved.txt
  - [ ] task-8-examples-compile.txt
  - [ ] task-8-non-ntag-untouched.txt
  - [ ] task-8-delegation.txt

  **Commit**: YES
  - Message: `refactor: convert Adafruit_PN532_NTAG424 to thin wrapper`
  - Files: `Adafruit_PN532_NTAG424.h`, `Adafruit_PN532_NTAG424.cpp`
  - Pre-commit: Both examples compile + all desktop tests pass

- [x] 9. Full regression verification

  **What to do**:
  - This is a verification-only task — no code changes
  - Run ALL verification checks to confirm the refactor is complete and correct:
    1. Compile both NTAG424 example sketches
    2. Run all desktop tests (existing `test_changekey_helpers` + new `test_ntag424_crypto` + new `test_ntag424_apdu`)
    3. Verify public API compatibility: count public NTAG424 methods, compare with baseline
    4. Verify include chain: `#include <Adafruit_PN532_NTAG424.h>` pulls in all types
    5. Verify no circular includes
    6. Verify all new files are in root directory
    7. Verify no PN532 references in reader-agnostic modules (crypto, apdu, core)
    8. Verify all "Must NOT Have" constraints are satisfied

  **Must NOT do**:
  - Do not make any code changes in this task
  - Do not fix issues found — report them for a fix task if needed

  **Recommended Agent Profile**:
  - **Category**: `unspecified-high`
    - Reason: Systematic verification requiring multiple checks but no architectural decisions
  - **Skills**: []

  **Parallelization**:
  - **Can Run In Parallel**: NO
  - **Parallel Group**: Wave 4 (sequential, after Task 8)
  - **Blocks**: Final Verification Wave
  - **Blocked By**: Task 8

  **References**:

  **Pattern References**:
  - This entire plan's "Must Have" and "Must NOT Have" sections — the verification checklist

  **API/Type References**:
  - Pre-refactor public API snapshot (capture before starting Task 1): `grep 'ntag424_' Adafruit_PN532_NTAG424.h`

  **WHY Each Reference Matters**:
  - The "Must Have"/"Must NOT Have" sections define exactly what to verify
  - Pre-refactor API snapshot is the baseline for compatibility comparison

  **Acceptance Criteria**:

  - [ ] Both example sketches compile without errors
  - [ ] All 3 desktop tests pass (changekey, crypto, apdu)
  - [ ] Public API method count unchanged
  - [ ] Include chain works (`#include <Adafruit_PN532_NTAG424.h>` sufficient)
  - [ ] No circular includes detected
  - [ ] All new files in root directory
  - [ ] Zero PN532 references in ntag424_crypto, ntag424_apdu, ntag424_core
  - [ ] All "Must NOT Have" constraints satisfied

  **QA Scenarios (MANDATORY):**

  ```
  Scenario: Full compilation regression
    Tool: Bash
    Preconditions: All files in place after Task 8
    Steps:
      1. Run: arduino-cli compile --fqbn esp32:esp32:esp32 examples/ntag424_examples/ntag424_examples.ino 2>&1
      2. Assert exit code 0
      3. Run: arduino-cli compile --fqbn esp32:esp32:esp32 examples/ntag424_isoreadfile/ntag424_isoreadfile.ino 2>&1
      4. Assert exit code 0
    Expected Result: Both examples compile cleanly
    Failure Indicators: Any compilation error
    Evidence: .sisyphus/evidence/task-9-compilation.txt

  Scenario: All desktop tests pass
    Tool: Bash
    Preconditions: All test files and source files exist
    Steps:
      1. Run: g++ -std=c++17 -I. -o test_changekey tests/test_changekey_helpers.cpp && ./test_changekey
      2. Assert exit code 0
      3. Run: g++ -std=c++17 -Itests/stubs -I. -o test_crypto tests/test_ntag424_crypto.cpp ntag424_crypto.cpp aescmac.cpp -lmbedcrypto && ./test_crypto
      4. Assert exit code 0
      5. Run: g++ -std=c++17 -Itests/stubs -I. -o test_apdu tests/test_ntag424_apdu.cpp ntag424_apdu.cpp ntag424_crypto.cpp aescmac.cpp -lmbedcrypto && ./test_apdu
      6. Assert exit code 0
    Expected Result: All 3 test suites pass
    Failure Indicators: Any test failure or compilation error
    Evidence: .sisyphus/evidence/task-9-tests.txt

  Scenario: Reader-agnostic modules have no PN532 coupling
    Tool: Bash
    Preconditions: All new files exist
    Steps:
      1. Run: grep -rl 'PN532\|pn532\|Adafruit\|sendCommandCheckAck\|readdata\|packetbuffer' ntag424_crypto.h ntag424_crypto.cpp ntag424_apdu.h ntag424_apdu.cpp ntag424_core.h ntag424_core.cpp ntag424_reader.h 2>&1
      2. Assert zero files returned (no PN532 coupling in reader-agnostic modules)
      3. Run: grep -rl 'PN532\|Adafruit' pn532_ntag424_adapter.h pn532_ntag424_adapter.cpp 2>&1
      4. Assert matches found (adapter is EXPECTED to reference PN532)
    Expected Result: Only the adapter references PN532; all other modules are clean
    Failure Indicators: PN532 references found in crypto/apdu/core/reader modules
    Evidence: .sisyphus/evidence/task-9-coupling.txt

  Scenario: File layout is flat
    Tool: Bash
    Preconditions: All files created
    Steps:
      1. Run: ls -1 ntag424_crypto.h ntag424_crypto.cpp ntag424_apdu.h ntag424_apdu.cpp ntag424_reader.h ntag424_core.h ntag424_core.cpp pn532_ntag424_adapter.h pn532_ntag424_adapter.cpp 2>&1
      2. Assert all 9 files listed (all in root directory)
      3. Run: find . -name 'ntag424_*' -not -path './tests/*' -not -path './.sisyphus/*' | sort
      4. Assert all paths start with './' (no subdirectories)
    Expected Result: All new files in project root, no subdirectories created
    Failure Indicators: Any file in a subdirectory
    Evidence: .sisyphus/evidence/task-9-flat-layout.txt
  ```

  **Evidence to Capture:**
  - [ ] task-9-compilation.txt
  - [ ] task-9-tests.txt
  - [ ] task-9-coupling.txt
  - [ ] task-9-flat-layout.txt

  **Commit**: NO (verification only, no file changes)

---

## Final Verification Wave

> 4 review agents run in PARALLEL. ALL must APPROVE. Present consolidated results to user and get explicit "okay" before completing.

- [x] F1. **Plan Compliance Audit** — `oracle`
  Read the plan end-to-end. For each "Must Have": verify implementation exists (read file, check method signatures). For each "Must NOT Have": search codebase for forbidden patterns — reject with file:line if found. Check evidence files exist in `.sisyphus/evidence/`. Compare deliverables against plan.
  Output: `Must Have [N/N] | Must NOT Have [N/N] | Tasks [N/N] | VERDICT: APPROVE/REJECT`

- [x] F2. **Code Quality Review** — `unspecified-high`
  Run syntax checks + desktop tests. Review all new/changed files for: `as any`/`@ts-ignore` equivalents in C++, empty catches, leftover debug prints in prod, commented-out code, unused includes. Check AI slop: excessive comments, over-abstraction, generic variable names.
  Output: `Build [PASS/FAIL] | Tests [N pass/N fail] | Files [N clean/N issues] | VERDICT`

- [x] F3. **Real Manual QA** — `unspecified-high`
  Start from clean state. Compile both NTAG424 example sketches. Run all desktop tests. Verify include chain: `#include <Adafruit_PN532_NTAG424.h>` pulls in all needed headers. Verify no circular includes. Check that `pn532_packetbuffer` is still accessible where needed.
  Output: `Compilation [N/N pass] | Tests [N/N] | Includes [CLEAN/ISSUES] | VERDICT`

- [x] F4. **Scope Fidelity Check** — `deep`
  For each task: read "What to do", read actual diff (git log/diff). Verify 1:1 — everything in spec was built (no missing), nothing beyond spec was built (no creep). Check "Must NOT do" compliance. Detect cross-task contamination. Flag unaccounted changes.
  Output: `Tasks [N/N compliant] | Contamination [CLEAN/N issues] | Unaccounted [CLEAN/N files] | VERDICT`

---

## Commit Strategy

| Order | Message | Files | Pre-commit Check |
|-------|---------|-------|-----------------|
| 1 | `refactor: extract ntag424_crypto.h/.cpp from monolith` | `ntag424_crypto.h`, `ntag424_crypto.cpp` | Syntax check passes |
| 2 | `test: add desktop tests for ntag424_crypto` | `tests/test_ntag424_crypto.cpp` | Test compiles and passes |
| 3 | `refactor: add ntag424_reader.h abstract interface` | `ntag424_reader.h` | Syntax check passes |
| 4 | `refactor: extract ntag424_apdu.h/.cpp from monolith` | `ntag424_apdu.h`, `ntag424_apdu.cpp` | Syntax check passes |
| 5 | `test: add desktop tests for ntag424_apdu` | `tests/test_ntag424_apdu.cpp` | Test compiles and passes |
| 6 | `refactor: extract ntag424_core.h/.cpp with session structs` | `ntag424_core.h`, `ntag424_core.cpp` | Syntax check passes |
| 7 | `refactor: add pn532_ntag424_adapter.h/.cpp` | `pn532_ntag424_adapter.h`, `pn532_ntag424_adapter.cpp` | Syntax check passes |
| 8 | `refactor: convert Adafruit_PN532_NTAG424 to thin wrapper` | `Adafruit_PN532_NTAG424.h`, `Adafruit_PN532_NTAG424.cpp` | Full compilation of both examples + all tests |
| 9 | `test: full regression verification` | — (no file changes, verification only) | All examples compile, all tests pass |

---

## Success Criteria

### Verification Commands
```bash
# Both example sketches compile cleanly
arduino-cli compile --fqbn esp32:esp32:esp32 examples/ntag424_examples/ntag424_examples.ino
arduino-cli compile --fqbn esp32:esp32:esp32 examples/ntag424_isoreadfile/ntag424_isoreadfile.ino

# Existing desktop test still passes
g++ -std=c++17 -o test_changekey tests/test_changekey_helpers.cpp && ./test_changekey

# New desktop tests pass
g++ -std=c++17 -Itests/stubs -I. -o test_crypto tests/test_ntag424_crypto.cpp ntag424_crypto.cpp aescmac.cpp -lmbedcrypto && ./test_crypto
g++ -std=c++17 -Itests/stubs -I. -o test_apdu tests/test_ntag424_apdu.cpp ntag424_apdu.cpp ntag424_crypto.cpp aescmac.cpp -lmbedcrypto && ./test_apdu

# Public API unchanged (count public methods)
grep -c 'ntag424_' Adafruit_PN532_NTAG424.h  # Same count as before refactor

# All files in root (no subdirectories created)
ls ntag424_crypto.h ntag424_apdu.h ntag424_reader.h ntag424_core.h pn532_ntag424_adapter.h
```

### Final Checklist
- [ ] All "Must Have" present — reader interface, crypto module, APDU module, core module, adapter, wrapper
- [ ] All "Must NOT Have" absent — no MFRC522, no subdirs, no API changes, no sketch changes
- [ ] Both example sketches compile
- [ ] All desktop tests pass (existing + new)
- [ ] Public method signatures unchanged
- [ ] Include chain works: `#include <Adafruit_PN532_NTAG424.h>` sufficient for all user code
