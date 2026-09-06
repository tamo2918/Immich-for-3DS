#include "sha1.h"
#include <stdio.h>
#include <string.h>

#define SHA1_ROL(value, bits) (((value) << (bits)) | ((value) >> (32 - (bits))))

#define BLK0(i) (block->l[i] = (SHA1_ROL(block->l[i], 24) & 0xFF00FF00) | (SHA1_ROL(block->l[i], 8) & 0x00FF00FF))
#define BLK(i) (block->l[i & 15] = SHA1_ROL(block->l[(i + 13) & 15] ^ block->l[(i + 8) & 15] ^ block->l[(i + 2) & 15] ^ block->l[i & 15], 1))

#define R0(v,w,x,y,z,i) z += ((w & (x ^ y)) ^ y) + BLK0(i) + 0x5A827999 + SHA1_ROL(v, 5); w = SHA1_ROL(w, 30);
#define R1(v,w,x,y,z,i) z += ((w & (x ^ y)) ^ y) + BLK(i) + 0x5A827999 + SHA1_ROL(v, 5); w = SHA1_ROL(w, 30);
#define R2(v,w,x,y,z,i) z += (w ^ x ^ y) + BLK(i) + 0x6ED9EBA1 + SHA1_ROL(v, 5); w = SHA1_ROL(w, 30);
#define R3(v,w,x,y,z,i) z += (((w | x) & y) | (w & x)) + BLK(i) + 0x8F1BBCDC + SHA1_ROL(v, 5); w = SHA1_ROL(w, 30);
#define R4(v,w,x,y,z,i) z += (w ^ x ^ y) + BLK(i) + 0xCA62C1D6 + SHA1_ROL(v, 5); w = SHA1_ROL(w, 30);

typedef union {
    uint8_t c[64];
    uint32_t l[16];
} CHAR64LONG16;

static void SHA1_Transform(uint32_t state[5], const uint8_t buffer[64]) {
    uint32_t a = state[0], b = state[1], c = state[2], d = state[3], e = state[4];
    CHAR64LONG16 block[1];
    memcpy(block, buffer, 64);

    R0(a, b, c, d, e, 0);  R0(e, a, b, c, d, 1);  R0(d, e, a, b, c, 2);  R0(c, d, e, a, b, 3);
    R0(b, c, d, e, a, 4);  R0(a, b, c, d, e, 5);  R0(e, a, b, c, d, 6);  R0(d, e, a, b, c, 7);
    R0(c, d, e, a, b, 8);  R0(b, c, d, e, a, 9);  R0(a, b, c, d, e, 10); R0(e, a, b, c, d, 11);
    R0(d, e, a, b, c, 12); R0(c, d, e, a, b, 13); R0(b, c, d, e, a, 14); R0(a, b, c, d, e, 15);
    R1(e, a, b, c, d, 16); R1(d, e, a, b, c, 17); R1(c, d, e, a, b, 18); R1(b, c, d, e, a, 19);
    R2(a, b, c, d, e, 20); R2(e, a, b, c, d, 21); R2(d, e, a, b, c, 22); R2(c, d, e, a, b, 23);
    R2(b, c, d, e, a, 24); R2(a, b, c, d, e, 25); R2(e, a, b, c, d, 26); R2(d, e, a, b, c, 27);
    R2(c, d, e, a, b, 28); R2(b, c, d, e, a, 29); R2(a, b, c, d, e, 30); R2(e, a, b, c, d, 31);
    R2(d, e, a, b, c, 32); R2(c, d, e, a, b, 33); R2(b, c, d, e, a, 34); R2(a, b, c, d, e, 35);
    R2(e, a, b, c, d, 36); R2(d, e, a, b, c, 37); R2(c, d, e, a, b, 38); R2(b, c, d, e, a, 39);
    R3(a, b, c, d, e, 40); R3(e, a, b, c, d, 41); R3(d, e, a, b, c, 42); R3(c, d, e, a, b, 43);
    R3(b, c, d, e, a, 44); R3(a, b, c, d, e, 45); R3(e, a, b, c, d, 46); R3(d, e, a, b, c, 47);
    R3(c, d, e, a, b, 48); R3(b, c, d, e, a, 49); R3(a, b, c, d, e, 50); R3(e, a, b, c, d, 51);
    R3(d, e, a, b, c, 52); R3(c, d, e, a, b, 53); R3(b, c, d, e, a, 54); R3(a, b, c, d, e, 55);
    R3(e, a, b, c, d, 56); R3(d, e, a, b, c, 57); R3(c, d, e, a, b, 58); R3(b, c, d, e, a, 59);
    R4(a, b, c, d, e, 60); R4(e, a, b, c, d, 61); R4(d, e, a, b, c, 62); R4(c, d, e, a, b, 63);
    R4(b, c, d, e, a, 64); R4(a, b, c, d, e, 65); R4(e, a, b, c, d, 66); R4(d, e, a, b, c, 67);
    R4(c, d, e, a, b, 68); R4(b, c, d, e, a, 69); R4(a, b, c, d, e, 70); R4(e, a, b, c, d, 71);
    R4(d, e, a, b, c, 72); R4(c, d, e, a, b, 73); R4(b, c, d, e, a, 74); R4(a, b, c, d, e, 75);
    R4(e, a, b, c, d, 76); R4(d, e, a, b, c, 77); R4(c, d, e, a, b, 78); R4(b, c, d, e, a, 79);

    state[0] += a;
    state[1] += b;
    state[2] += c;
    state[3] += d;
    state[4] += e;
}

void SHA1_Init(SHA1_CTX* context) {
    context->state[0] = 0x67452301;
    context->state[1] = 0xEFCDAB89;
    context->state[2] = 0x98BADCFE;
    context->state[3] = 0x10325476;
    context->state[4] = 0xC3D2E1F0;
    context->count[0] = context->count[1] = 0;
}

void SHA1_Update(SHA1_CTX* context, const uint8_t* data, size_t len) {
    size_t i, j;
    j = (context->count[0] >> 3) & 63;
    if ((context->count[0] += (uint32_t)(len << 3)) < (len << 3))
        context->count[1]++;
    context->count[1] += (uint32_t)(len >> 29);

    if ((j + len) > 63) {
        memcpy(&context->buffer[j], data, (i = 64 - j));
        SHA1_Transform(context->state, context->buffer);
        for (; i + 63 < len; i += 64) {
            SHA1_Transform(context->state, &data[i]);
        }
        j = 0;
    } else {
        i = 0;
    }
    memcpy(&context->buffer[j], &data[i], len - i);
}

void SHA1_Final(uint8_t digest[20], SHA1_CTX* context) {
    uint32_t i;
    uint8_t finalcount[8];
    for (i = 0; i < 8; i++) {
        finalcount[i] = (uint8_t)((context->count[(i >= 4 ? 0 : 1)] >> ((3 - (i & 3)) * 8)) & 255);
    }
    SHA1_Update(context, (const uint8_t*)"\x80", 1);
    while ((context->count[0] & 504) != 448) {
        SHA1_Update(context, (const uint8_t*)"\0", 1);
    }
    SHA1_Update(context, finalcount, 8);
    for (i = 0; i < 20; i++) {
        digest[i] = (uint8_t)((context->state[i >> 2] >> ((3 - (i & 3)) * 8)) & 255);
    }
    memset(context, 0, sizeof(*context));
}

void SHA1_ToHex(const uint8_t digest[20], char hex_out[41]) {
    static const char hexchars[] = "0123456789abcdef";
    for (int i = 0; i < 20; i++) {
        hex_out[i * 2]     = hexchars[(digest[i] >> 4) & 0x0F];
        hex_out[i * 2 + 1] = hexchars[digest[i] & 0x0F];
    }
    hex_out[40] = '\0';
}

void SHA1_ToBase64(const uint8_t digest[20], char base64_out[29]) {
    static const char b64table[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
    int i = 0, j = 0;
    for (i = 0; i < 18; i += 3) {
        uint32_t octet_a = digest[i];
        uint32_t octet_b = digest[i + 1];
        uint32_t octet_c = digest[i + 2];
        uint32_t triple = (octet_a << 16) | (octet_b << 8) | octet_c;
        base64_out[j++] = b64table[(triple >> 18) & 0x3F];
        base64_out[j++] = b64table[(triple >> 12) & 0x3F];
        base64_out[j++] = b64table[(triple >> 6) & 0x3F];
        base64_out[j++] = b64table[triple & 0x3F];
    }
    // Remaining 2 bytes (20 total)
    uint32_t octet_a = digest[18];
    uint32_t octet_b = digest[19];
    uint32_t triple = (octet_a << 16) | (octet_b << 8);
    base64_out[j++] = b64table[(triple >> 18) & 0x3F];
    base64_out[j++] = b64table[(triple >> 12) & 0x3F];
    base64_out[j++] = b64table[(triple >> 6) & 0x3F];
    base64_out[j++] = '=';
    base64_out[j] = '\0';
}

int SHA1_FileHex(const char* filepath, char hex_out[41]) {
    FILE* f = fopen(filepath, "rb");
    if (!f) return -1;

    SHA1_CTX ctx;
    SHA1_Init(&ctx);

    // Use 32KB buffer for efficient SD card read
    uint8_t buffer[32768];
    size_t bytes_read;
    while ((bytes_read = fread(buffer, 1, sizeof(buffer), f)) > 0) {
        SHA1_Update(&ctx, buffer, bytes_read);
    }
    fclose(f);

    uint8_t digest[20];
    SHA1_Final(digest, &ctx);
    SHA1_ToHex(digest, hex_out);
    return 0;
}
