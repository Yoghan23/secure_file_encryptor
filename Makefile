CC = gcc
CFLAGS = -Wall -Wextra -O2

# Detect Homebrew OpenSSL path on macOS
BREW_OPENSSL_ARM = /opt/homebrew/opt/openssl@3
BREW_OPENSSL_INTEL = /usr/local/opt/openssl@3

ifneq ($(wildcard $(BREW_OPENSSL_ARM)/include),)
    CFLAGS += -I$(BREW_OPENSSL_ARM)/include
    LDFLAGS += -L$(BREW_OPENSSL_ARM)/lib -lcrypto
else ifneq ($(wildcard $(BREW_OPENSSL_INTEL)/include),)
    CFLAGS += -I$(BREW_OPENSSL_INTEL)/include
    LDFLAGS += -L$(BREW_OPENSSL_INTEL)/lib -lcrypto
else
    CFLAGS += $(shell pkg-config --cflags openssl 2>/dev/null)
    LDFLAGS += $(shell pkg-config --libs openssl 2>/dev/null || echo "-lcrypto")
endif

TARGET = encryptor
SRC = src/main.c

all: $(TARGET)

$(TARGET): $(SRC)
	$(CC) $(CFLAGS) $(SRC) -o $(TARGET) $(LDFLAGS)

test: $(TARGET)
	./test.sh

clean:
	rm -f $(TARGET) encrypted.bin decrypted.txt
	rm -rf test_tmp

.PHONY: all clean test