#ifndef SHA1_H
#define SHA1_H

#include <stdint.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    uint32_t state[5];
    uint32_t count[2];
    uint8_t buffer[64];
} SHA1_CTX;

void SHA1_Init(SHA1_CTX* context);
void SHA1_Update(SHA1_CTX* context, const uint8_t* data, size_t len);
void SHA1_Final(uint8_t digest[20], SHA1_CTX* context);

// Helper: Calculate SHA1 of a file, outputs 40-char hex string (lowercase) null-terminated
// Returns 0 on success, -1 on file open error
int SHA1_FileHex(const char* filepath, char hex_out[41]);

// Helper: Convert 20-byte digest to 40-char hex string
void SHA1_ToHex(const uint8_t digest[20], char hex_out[41]);

// Helper: Convert 20-byte digest to Base64 string (28 chars + null terminator = 29 chars)
void SHA1_ToBase64(const uint8_t digest[20], char base64_out[29]);

#ifdef __cplusplus
}
#endif

#endif // SHA1_H
