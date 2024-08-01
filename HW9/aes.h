//
// Created by Admin on 12/8/2023.
//

#ifndef AES_H
#define AES_H

#include <openssl/aes.h>
#include <openssl/rand.h>

int encrypt_aes(const unsigned char *plaintext, unsigned char *ciphertext, const unsigned char *key) {
    AES_KEY aesKey;
    AES_set_encrypt_key(key, 128, &aesKey);

    size_t input_length = strlen(plaintext);
    size_t num_blocks = (input_length + AES_BLOCK_SIZE - 1) / AES_BLOCK_SIZE;

    for (size_t i = 0; i < num_blocks; ++i) {
        size_t block_start = i * AES_BLOCK_SIZE;
        size_t block_length = AES_BLOCK_SIZE;
        if (i == num_blocks - 1) {
            // For the last block, adjust the length to avoid reading beyond the end of the string
            block_length = input_length - block_start;
        }

        AES_encrypt((const unsigned char *) (plaintext + block_start), ciphertext + block_start, &aesKey);
    }

    return num_blocks * AES_BLOCK_SIZE;
}

int decrypt_aes(const unsigned char *ciphertext, unsigned char *plaintext, const unsigned char *key) {
    AES_KEY aesKey;
    AES_set_decrypt_key(key, 128, &aesKey);

    size_t input_length = strlen((const char *) ciphertext);
    if (input_length % AES_BLOCK_SIZE != 0) input_length = 1024;
    size_t num_blocks = (input_length + AES_BLOCK_SIZE - 1) / AES_BLOCK_SIZE;

    for (size_t i = 0; i < num_blocks; ++i) {
        size_t block_start = i * AES_BLOCK_SIZE;
        size_t block_length = AES_BLOCK_SIZE;
        if (i == num_blocks - 1) {
            // For the last block, adjust the length to avoid reading beyond the end of the ciphertext
            block_length = input_length - block_start;
        }

        AES_decrypt(ciphertext + block_start, plaintext + block_start, &aesKey);
    }

    return num_blocks * AES_BLOCK_SIZE;
}

#endif // AES_H
