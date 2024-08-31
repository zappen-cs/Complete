#include "crypto.h"

void xor_encrypt_decrypt(char *data, int data_len, const char *key, int key_len) {
    if (key_len == 0) {
        printf("Error: Key length is zero.\n");
        return;
    }

    for (int i = 0; i < data_len; i++) {
        data[i] = data[i] ^ key[i % key_len];
    }
}
