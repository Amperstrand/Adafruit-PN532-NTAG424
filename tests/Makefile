CXX ?= g++

CXXFLAGS := -std=c++17 -Wall -Wextra -pedantic -I.. -I../tests/stubs
LDLIBS := -lmbedtls -lmbedcrypto -lmbedx509

COMMON_CRYPTO_SRCS := ../ntag424_crypto.cpp ../aescmac.cpp host_stubs.cpp
CORE_SRCS := ../ntag424_apdu.cpp ../ntag424_core.cpp ../ntag424_handler.cpp

TEST_BINS := \
	test_ntag424_apdu \
	test_ntag424_core \
	test_ntag424_crypto \
	test_changekey_helpers \
	test_public_header \
	test_mfrc522_public_header \
	test_mock_transceiver

.PHONY: all test clean

all: test

test: $(TEST_BINS)
	@set -e; for test_bin in $(TEST_BINS); do ./$$test_bin; done

test_ntag424_apdu: test_ntag424_apdu.cpp ../ntag424_apdu.cpp ../ntag424_crypto.cpp ../aescmac.cpp host_stubs.cpp
	$(CXX) $(CXXFLAGS) -o $@ $^ $(LDLIBS)

test_ntag424_core: test_ntag424_core.cpp $(CORE_SRCS) $(COMMON_CRYPTO_SRCS)
	$(CXX) $(CXXFLAGS) -o $@ $^ $(LDLIBS)

test_ntag424_crypto: test_ntag424_crypto.cpp $(COMMON_CRYPTO_SRCS)
	$(CXX) $(CXXFLAGS) -o $@ $^ $(LDLIBS)

test_changekey_helpers: test_changekey_helpers.cpp
	$(CXX) $(CXXFLAGS) -o $@ $<

test_public_header: test_public_header.cpp ../Adafruit_PN532_NTAG424.cpp ../ntag424_handler.cpp ../ntag424_apdu.cpp ../ntag424_core.cpp $(COMMON_CRYPTO_SRCS)
	$(CXX) $(CXXFLAGS) -o $@ $^ $(LDLIBS)

test_mfrc522_public_header: test_mfrc522_public_header.cpp ../MFRC522_NTAG424.cpp ../MFRC522_I2C.cpp ../MFRC522_I2C_Extended.cpp ../mfrc522_ntag424_adapter.cpp ../ntag424_handler.cpp ../ntag424_apdu.cpp ../ntag424_core.cpp $(COMMON_CRYPTO_SRCS)
	$(CXX) $(CXXFLAGS) -o $@ $^ $(LDLIBS)

test_mock_transceiver: test_mock_transceiver.cpp ../ntag424_core.cpp ../ntag424_apdu.cpp $(COMMON_CRYPTO_SRCS)
	$(CXX) $(CXXFLAGS) -o $@ $^ $(LDLIBS)

clean:
	rm -f $(TEST_BINS)
