# Secure C File Encryptor (AES-256-GCM)

![C](https://img.shields.io/badge/Language-C11-blue.svg)
![OpenSSL](https://img.shields.io/badge/Library-OpenSSL_v3.0%2B-red.svg)
![Security](https://img.shields.io/badge/Cipher-AES--256--GCM-green.svg)
![Build](https://img.shields.io/badge/Build-Passing-brightgreen.svg)

A production-ready command-line tool written in **C11** that performs authenticated symmetric file encryption and decryption.

Built on OpenSSL `libcrypto`, it uses **AES-256-GCM** to provide confidentiality, data integrity, and tamper detection while supporting files of arbitrary size through streamed processing.

---

## Features

* **AES-256-GCM** authenticated encryption
* **PBKDF2-HMAC-SHA256** key derivation
* **128-bit cryptographically secure random salts**
* **96-bit random initialization vectors (IVs)**
* **128-bit authentication tags**
* **64 KiB streamed file processing**
* **Constant-memory processing** with respect to file size
* **Hidden password input** using `termios`
* **Sensitive-memory cleanup** using OpenSSL
* **Automatic rejection of tampered ciphertext**
* **Automated integration testing**
* Designed for Unix-like systems including Linux and macOS

---

## Technical Highlights & Architecture

### Authenticated Encryption

The application uses **AES-256-GCM (Galois/Counter Mode)**.

Unlike unauthenticated encryption modes such as CBC or CTR, GCM provides both:

* Confidentiality
* Integrity
* Authentication
* Tamper detection

A 128-bit authentication tag is generated during encryption and verified during decryption.

If the ciphertext, IV, authentication tag, or password-derived key does not match, authentication fails and the decrypted output is rejected.

---

### Key Derivation

User passwords are never used directly as AES keys.

Instead, the application derives a 256-bit encryption key using:

```text
PBKDF2-HMAC-SHA256
```

The key derivation process uses:

* **256-bit derived key**
* **128-bit random salt**
* **100,000 iterations**
* **HMAC-SHA256**

The random salt ensures that the same password produces different derived keys for different encrypted files.

> **Note:** The PBKDF2 iteration count should be reviewed and benchmarked for modern production deployments. The value used by this project is primarily a design choice for the implementation.

---

### Streamed File Processing

Files are processed in **64 KiB chunks** rather than being loaded completely into memory.

This allows the program to encrypt and decrypt very large files while maintaining a small memory footprint.

The approximate complexity is:

```text
Time:   O(N)
Memory: O(1) with respect to file size
```

Where `N` is the size of the input file.

---

## Encryption Architecture

```text
                    User Password
                          |
                          v
                +-------------------+
                |      PBKDF2       |
                |   HMAC-SHA256     |
                +---------+---------+
                          |
                          v
                    256-bit Key
                          |
                          v
Plaintext ----------> AES-256-GCM ----------> Ciphertext
                          |
                          v
                    Auth Tag (16 B)
```

During encryption:

1. A cryptographically secure random salt is generated.
2. A random 96-bit IV is generated.
3. PBKDF2 derives a 256-bit key from the user's password and salt.
4. The plaintext is processed in 64 KiB chunks.
5. AES-256-GCM encrypts the plaintext.
6. A 128-bit authentication tag is generated.
7. The salt, IV, ciphertext, and authentication tag are written to the output file.

During decryption, the authentication tag must verify successfully before the decrypted data is considered valid.

---

## Encrypted File Format

The encrypted file uses the following binary structure:

```text
+-------------+------------+----------------+-------------+
|    Salt     |     IV     |   Ciphertext   |  Auth Tag   |
|    16 B     |    12 B    |      N B       |    16 B     |
+-------------+------------+----------------+-------------+
```

In compact form:

```text
Payload = Salt || IV || Ciphertext || Auth Tag
```

### File Format

| Field      |     Size | Description                   |
| ---------- | -------: | ----------------------------- |
| Salt       | 16 bytes | Random PBKDF2 salt            |
| IV         | 12 bytes | AES-GCM initialization vector |
| Ciphertext |  N bytes | Encrypted file contents       |
| Auth Tag   | 16 bytes | GCM authentication tag        |

The salt and IV are not secret and are stored alongside the ciphertext.

The user's password is required to derive the encryption key.

---

## Memory Hygiene & Terminal Security

Password input is hidden from the terminal using `termios`.

Sensitive cryptographic material is cleared from memory after use using OpenSSL's memory-cleansing functionality.

This reduces the amount of sensitive key material left in process memory after cryptographic operations have completed.

---

## Project Structure

```text
secure_file_encryptor/
├── src/
│   └── main.c          # Core application logic and OpenSSL bindings
├── Makefile            # Build configuration
├── test.sh             # Automated integration test suite
└── README.md           # Project documentation
```

---

## Prerequisites

The following software is required:

* **C Compiler:** GCC or Clang
* **C Standard:** C11
* **OpenSSL:** Version 3.0+
* **libcrypto**
* **make**
* **pkg-config**

---

## Installation

### macOS

Install the required dependencies using Homebrew:

```bash
brew install openssl@3 pkg-config
```

Then build the project:

```bash
make
```

Depending on your Homebrew installation, you may need to ensure that OpenSSL is correctly exposed to the compiler and linker.

---

### Ubuntu / Debian / WSL

Install the required development packages:

```bash
sudo apt update
sudo apt install build-essential libssl-dev pkg-config
```

Then build the project:

```bash
make
```

---

## Building the Project

Compile the executable using the provided `Makefile`:

```bash
make
```

This produces the executable:

```text
encryptor
```

To remove build artifacts:

```bash
make clean
```

---

## Usage

### Encrypting a File

To encrypt a file:

```bash
./encryptor -e plain.txt encrypted.bin
```

You will be prompted to enter your password.

The password input is hidden from the terminal.

Example:

```text
Enter password:
Confirm password:
```

The encrypted output will be written to:

```text
encrypted.bin
```

---

### Decrypting a File

To decrypt an encrypted file:

```bash
./encryptor -d encrypted.bin decrypted.txt
```

Enter the password used during encryption.

If authentication succeeds, the plaintext is written to:

```text
decrypted.txt
```

If the password is incorrect or the encrypted file has been modified, authentication fails and the decrypted output is safely discarded.

---

## Authentication & Tamper Detection

AES-256-GCM provides authenticated encryption.

For example, if an attacker modifies a byte of the ciphertext:

```text
Original Ciphertext
        |
        v
   AES-256-GCM
        |
        v
 Authentication
        |
        v
      VALID
```

After tampering:

```text
Modified Ciphertext
        |
        v
   AES-256-GCM
        |
        v
 Authentication
        |
        v
     FAILURE
        |
        v
 Output discarded
```

This prevents modified ciphertext from being silently decrypted as valid plaintext.

---

## Security Properties

| Property             | Implementation           |
| -------------------- | ------------------------ |
| Encryption Algorithm | AES-256-GCM              |
| Authentication       | GCM Authentication Tag   |
| Key Derivation       | PBKDF2-HMAC-SHA256       |
| Derived Key Size     | 256 bits                 |
| Salt Size            | 128 bits                 |
| IV Size              | 96 bits                  |
| Authentication Tag   | 128 bits                 |
| Processing           | 64 KiB Streaming         |
| Password Input       | Hidden Terminal Input    |
| Memory Cleanup       | OpenSSL Memory Cleansing |
| Tamper Detection     | AES-GCM Authentication   |

---

## Automated Integration Testing

The project includes an end-to-end regression test suite:

```text
test.sh
```

Run the test suite using:

```bash
make test
```

The test suite verifies:

1. Successful file encryption
2. Successful file decryption
3. Exact plaintext round-trip integrity
4. Rejection of an incorrect password
5. Detection of tampered ciphertext

### Example Output

```text
==================================================
 Running Automated Integration Tests for Encryptor
==================================================
[PASS] 1. File Encryption Execution
[PASS] 2. File Decryption Execution
[PASS] 3. Decrypted Payload Matches Original Content Exactly
[PASS] 4. Authentication Rejection on Wrong Password
[PASS] 5. AEAD Tag Detection on Tampered Ciphertext
==================================================
 Test Results: 5/5 Passed
==================================================
```

---

## Threat Model

The application is designed to protect encrypted files from attackers who do not possess the correct password.

It provides protection against:

* Unauthorized access to encrypted files
* Offline modification of ciphertext
* Accidental ciphertext corruption
* Undetected bit-flipping attacks
* Password attacks assisted by precomputed rainbow tables

It does **not** protect against:

* A compromised operating system
* Malware capable of capturing the password
* An attacker who already has access to the plaintext
* Weak or predictable passwords
* Compromised process memory
* Vulnerabilities in the implementation or its dependencies
* Loss of the encryption password

---

## Password Security

The strength of the encryption ultimately depends on the password used by the user.

Use a strong, unique password or passphrase.

For example:

```text
correct-horse-battery-staple-example
```

is significantly preferable to:

```text
password123
```

Avoid:

* Short passwords
* Common passwords
* Dictionary words
* Reused passwords
* Personal information

There is no password recovery mechanism. Losing the password may result in permanent loss of access to the encrypted data.

---

## Important Security Considerations

### Original Plaintext

Encrypting a file does not automatically remove the original plaintext.

For example:

```text
plain.txt
encrypted.bin
```

Both files may exist after encryption.

If the plaintext must be securely removed, the appropriate procedure depends on the storage medium and your threat model.

### Password Protection

The password should never be stored alongside the encrypted file.

Anyone who obtains both the encrypted file and the password may be able to decrypt the contents.

### Security Review

This project demonstrates practical cryptographic engineering using OpenSSL.

Passing automated tests does **not** constitute a formal security audit.

Before relying on the application to protect highly sensitive or high-value information, the implementation should undergo an independent security review.

---

## Limitations

The current file format is intentionally simple:

```text
Salt || IV || Ciphertext || Auth Tag
```

The current implementation does not provide:

* Password recovery
* Key-file support
* Multiple encryption keys
* Multiple recipients
* File metadata preservation
* File format versioning
* Hardware-backed key storage
* Built-in password strength enforcement
* Authenticated file metadata

---

