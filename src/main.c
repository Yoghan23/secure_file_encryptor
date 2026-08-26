#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <termios.h>
#include <unistd.h>

#include <openssl/evp.h>
#include <openssl/rand.h>
#include <openssl/err.h>

#define SALT_LEN 16
#define IV_LEN 12
#define TAG_LEN 16
#define KEY_LEN 32
#define KDF_ITERATIONS 100000
#define BUFFER_SIZE 65536

// Read password without echoing to terminal
void get_password(const char *prompt, char *buffer, size_t max_len) {
    // If stdin is a interactive terminal, disable terminal echo
    if (isatty(STDIN_FILENO)) {
        struct termios oldt, newt;
        printf("%s", prompt);
        fflush(stdout);

        tcgetattr(STDIN_FILENO, &oldt);
        newt = oldt;
        newt.c_lflag &= ~(ECHO);
        tcsetattr(STDIN_FILENO, TCSANOW, &newt);

        if (fgets(buffer, max_len, stdin) != NULL) {
            size_t len = strlen(buffer);
            if (len > 0 && buffer[len - 1] == '\n') {
                buffer[len - 1] = '\0';
            }
        }

        tcsetattr(STDIN_FILENO, TCSANOW, &oldt);
        printf("\n");
    } else {
        // Non-interactive (pipe/redirect mode for scripts/testing)
        if (fgets(buffer, max_len, stdin) != NULL) {
            size_t len = strlen(buffer);
            if (len > 0 && buffer[len - 1] == '\n') {
                buffer[len - 1] = '\0';
            }
        }
    }
}

int derive_key(const char *password, const unsigned char *salt, unsigned char *key) {
    return PKCS5_PBKDF2_HMAC(password, strlen(password),
                             salt, SALT_LEN,
                             KDF_ITERATIONS,
                             EVP_sha256(),
                             KEY_LEN, key);
}

int encrypt_file(const char *in_file, const char *out_file, const char *password) {
    FILE *fin = fopen(in_file, "rb");
    FILE *fout = fopen(out_file, "wb");
    if (!fin || !fout) {
        perror("File opening failed");
        if (fin) fclose(fin);
        if (fout) fclose(fout);
        return 0;
    }

    unsigned char salt[SALT_LEN];
    unsigned char iv[IV_LEN];
    unsigned char key[KEY_LEN];
    unsigned char tag[TAG_LEN];

    if (RAND_bytes(salt, SALT_LEN) != 1 || RAND_bytes(iv, IV_LEN) != 1) {
        fprintf(stderr, "Error generating random bytes.\n");
        fclose(fin); fclose(fout);
        return 0;
    }

    if (!derive_key(password, salt, key)) {
        fprintf(stderr, "Key derivation failed.\n");
        fclose(fin); fclose(fout);
        return 0;
    }

    // Write Salt and IV to output header
    fwrite(salt, 1, SALT_LEN, fout);
    fwrite(iv, 1, IV_LEN, fout);

    EVP_CIPHER_CTX *ctx = EVP_CIPHER_CTX_new();
    EVP_EncryptInit_ex(ctx, EVP_aes_256_gcm(), NULL, NULL, NULL);
    EVP_CIPHER_CTX_ctrl(ctx, EVP_CTRL_GCM_SET_IVLEN, IV_LEN, NULL);
    EVP_EncryptInit_ex(ctx, NULL, NULL, key, iv);

    unsigned char in_buf[BUFFER_SIZE];
    unsigned char out_buf[BUFFER_SIZE + EVP_MAX_BLOCK_LENGTH];
    int bytes_read, out_len;

    while ((bytes_read = fread(in_buf, 1, BUFFER_SIZE, fin)) > 0) {
        if (!EVP_EncryptUpdate(ctx, out_buf, &out_len, in_buf, bytes_read)) {
            fprintf(stderr, "Encryption error.\n");
            EVP_CIPHER_CTX_free(ctx);
            fclose(fin); fclose(fout);
            return 0;
        }
        fwrite(out_buf, 1, out_len, fout);
    }

    EVP_EncryptFinal_ex(ctx, out_buf, &out_len);
    fwrite(out_buf, 1, out_len, fout);

    // Get Auth Tag and write to header end
    EVP_CIPHER_CTX_ctrl(ctx, EVP_CTRL_GCM_GET_TAG, TAG_LEN, tag);
    fwrite(tag, 1, TAG_LEN, fout);

    // Secure Cleanup
    OPENSSL_cleanse(key, KEY_LEN);
    EVP_CIPHER_CTX_free(ctx);
    fclose(fin);
    fclose(fout);
    return 1;
}

int decrypt_file(const char *in_file, const char *out_file, const char *password) {
    FILE *fin = fopen(in_file, "rb");
    FILE *fout = fopen(out_file, "wb");
    if (!fin || !fout) {
        perror("File opening failed");
        if (fin) fclose(fin);
        if (fout) fclose(fout);
        return 0;
    }

    unsigned char salt[SALT_LEN];
    unsigned char iv[IV_LEN];
    unsigned char key[KEY_LEN];
    unsigned char tag[TAG_LEN];

    // Read Salt and IV from header
    if (fread(salt, 1, SALT_LEN, fin) != SALT_LEN || fread(iv, 1, IV_LEN, fin) != IV_LEN) {
        fprintf(stderr, "Invalid or corrupted file header.\n");
        fclose(fin); fclose(fout);
        return 0;
    }

    // Extract Tag from the end of the input file
    fseek(fin, -TAG_LEN, SEEK_END);
    long ciphertext_end = ftell(fin);
    fread(tag, 1, TAG_LEN, fin);

    // Reset stream position to right after IV
    fseek(fin, SALT_LEN + IV_LEN, SEEK_SET);

    if (!derive_key(password, salt, key)) {
        fprintf(stderr, "Key derivation failed.\n");
        fclose(fin); fclose(fout);
        return 0;
    }

    EVP_CIPHER_CTX *ctx = EVP_CIPHER_CTX_new();
    EVP_DecryptInit_ex(ctx, EVP_aes_256_gcm(), NULL, NULL, NULL);
    EVP_CIPHER_CTX_ctrl(ctx, EVP_CTRL_GCM_SET_IVLEN, IV_LEN, NULL);
    EVP_DecryptInit_ex(ctx, NULL, NULL, key, iv);

    unsigned char in_buf[BUFFER_SIZE];
    unsigned char out_buf[BUFFER_SIZE + EVP_MAX_BLOCK_LENGTH];
    int out_len;
    long bytes_remaining = ciphertext_end - (SALT_LEN + IV_LEN);

    while (bytes_remaining > 0) {
        int to_read = (bytes_remaining > BUFFER_SIZE) ? BUFFER_SIZE : bytes_remaining;
        int bytes_read = fread(in_buf, 1, to_read, fin);
        
        if (!EVP_DecryptUpdate(ctx, out_buf, &out_len, in_buf, bytes_read)) {
            fprintf(stderr, "Decryption processing failed.\n");
            EVP_CIPHER_CTX_free(ctx);
            fclose(fin); fclose(fout);
            return 0;
        }
        fwrite(out_buf, 1, out_len, fout);
        bytes_remaining -= bytes_read;
    }

    // Set Expected Auth Tag
    EVP_CIPHER_CTX_ctrl(ctx, EVP_CTRL_GCM_SET_TAG, TAG_LEN, tag);

    // Finalize Decryption and Verify Tag
    int ret = EVP_DecryptFinal_ex(ctx, out_buf, &out_len);
    if (ret > 0) {
        fwrite(out_buf, 1, out_len, fout);
    } else {
        fprintf(stderr, "Authentication failed! Invalid password or corrupted file.\n");
        // Remove output file if decryption/auth fails
        remove(out_file);
    }

    OPENSSL_cleanse(key, KEY_LEN);
    EVP_CIPHER_CTX_free(ctx);
    fclose(fin);
    fclose(fout);

    return ret > 0;
}

int main(int argc, char *argv[]) {
    if (argc != 4) {
        printf("Usage: %s <-e|-d> <input_file> <output_file>\n", argv[0]);
        printf("  -e : Encrypt file\n");
        printf("  -d : Decrypt file\n");
        return 1;
    }

    char password[256];
    get_password("Enter Master Password: ", password, sizeof(password));

    int success = 0;
    if (strcmp(argv[1], "-e") == 0) {
        success = encrypt_file(argv[2], argv[3], password);
        if (success) {
            printf("File encrypted successfully to '%s'.\n", argv[3]);
        }
    } else if (strcmp(argv[1], "-d") == 0) {
        success = decrypt_file(argv[2], argv[3], password);
        if (success) {
            printf("File decrypted successfully to '%s'.\n", argv[3]);
        }
    } else {
        fprintf(stderr, "Invalid option. Use -e or -d.\n");
        OPENSSL_cleanse(password, sizeof(password));
        return 1;
    }

    OPENSSL_cleanse(password, sizeof(password));
    return success ? 0 : 1; // Return non-zero status code on failure
}