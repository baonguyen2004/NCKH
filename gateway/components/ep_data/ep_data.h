#ifndef EP_DATA_H
#define EP_DATA_H

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

// Max length (including NUL) for barcode buffer
#ifndef EP_BARCODE_MAX_LEN
#define EP_BARCODE_MAX_LEN 32
#endif

typedef struct {
    uint16_t add;                          // Example: 0x005B
    int32_t  price;                        // Example: 150000 (VND)
    char     barcode[EP_BARCODE_MAX_LEN];  // NUL-terminated
} EPData;

typedef enum {
    EP_OK = 0,
    EP_ERR_NULL     = -1,
    EP_ERR_KEY      = -2,
    EP_ERR_VALUE    = -3,
    EP_ERR_OVERFLOW = -4,
    EP_ERR_FORMAT   = -5
} EPStatus;

/**
 * Parse JSON like:
 * {"add":"0x005B","price":150000,"barcode":"0123456789012"}
 * - add: string "0x...." (hex) OR decimal number
 * - price: integer number
 * - barcode: quoted string
 */
EPStatus ep_parse(const char *json, EPData *out);

/**
 * Serialize to JSON. Example output:
 * {"add":"0x005B","price":150000,"barcode":"0123456789012"}
 * Returns number of bytes written (excluding NUL) on success.
 * Returns negative (EP_ERR_*) on error (e.g., buffer too small).
 */
int ep_to_json(const EPData *in, char *buf, size_t buflen);

/**
 * Basic validation:
 * - price >= 0
 * - barcode contains only digits and length 1..(EP_BARCODE_MAX_LEN-1)
 * - if barcode length == 13, also check EAN-13 checksum
 */
bool ep_validate(const EPData *in);

/**
 * Verify EAN-13 checksum for a 13-digit string.
 */
bool ep_ean13_verify(const char *barcode);

#ifdef __cplusplus
}
#endif

#endif // EP_DATA_H