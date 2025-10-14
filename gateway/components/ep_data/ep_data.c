#include "ep_data.h"
#include <string.h>
#include <stdlib.h>
#include <ctype.h>
#include <errno.h>
#include <stdio.h>

static const char* skip_ws(const char* p) {
    while (p && *p && isspace((unsigned char)*p)) ++p;
    return p;
}

static const char* find_key_value_start(const char* json, const char* key) {
    if (!json || !key) return NULL;
    char pattern[96];
    // Build pattern: "key"
    if (snprintf(pattern, sizeof(pattern), "\"%s\"", key) < 0) return NULL;
    const char* p = json;
    size_t pat_len = strlen(pattern);
    while ((p = strstr(p, pattern)) != NULL) {
        const char* q = p + pat_len;
        q = skip_ws(q);
        if (*q == ':') {
            return skip_ws(q + 1);
        }
        p = q;
    }
    return NULL;
}

static int parse_string(const char* v, char* out, size_t outlen) {
    if (!v || !out || outlen == 0) return EP_ERR_NULL;
    if (*v != '\"') return EP_ERR_FORMAT;
    v++; // skip opening quote
    size_t i = 0;
    int escape = 0;
    while (*v) {
        char c = *v++;
        if (escape) {
            // Minimal escape handling: just copy the escaped char
            if (i + 1 >= outlen) return EP_ERR_OVERFLOW;
            out[i++] = c;
            escape = 0;
        } else if (c == '\\') {
            escape = 1;
        } else if (c == '\"') {
            out[i] = '\0';
            return EP_OK;
        } else {
            if (i + 1 >= outlen) return EP_ERR_OVERFLOW;
            out[i++] = c;
        }
    }
    return EP_ERR_FORMAT; // no closing quote
}

static int parse_int32(const char* v, int32_t* out) {
    if (!v || !out) return EP_ERR_NULL;
    errno = 0;
    char* endptr = NULL;
    long val = strtol(v, &endptr, 10);
    if (errno != 0 || endptr == v) return EP_ERR_VALUE;
    if (val < INT32_MIN || val > INT32_MAX) return EP_ERR_OVERFLOW;
    *out = (int32_t)val;
    return EP_OK;
}

static int parse_add_auto(const char* v, uint16_t* out) {
    if (!v || !out) return EP_ERR_NULL;
    if (*v == '\"') {
        char tmp[32];
        int s = parse_string(v, tmp, sizeof(tmp));
        if (s != EP_OK) return s;
        errno = 0;
        char* endptr = NULL;
        unsigned long x = strtoul(tmp, &endptr, 0); // base 0 handles "0x" hex
        if (errno != 0 || endptr == tmp || *endptr != '\0' || x > 0xFFFFu) return EP_ERR_VALUE;
        *out = (uint16_t)x;
        return EP_OK;
    } else {
        errno = 0;
        char* endptr = NULL;
        long val = strtol(v, &endptr, 10);
        if (errno != 0 || endptr == v || val < 0 || val > 0xFFFF) return EP_ERR_VALUE;
        *out = (uint16_t)val;
        return EP_OK;
    }
}

bool ep_ean13_verify(const char *barcode) {
    if (!barcode) return false;
    size_t len = strlen(barcode);
    if (len != 13) return false;
    int sum = 0;
    for (size_t i = 0; i < 12; ++i) {
        if (!isdigit((unsigned char)barcode[i])) return false;
        int d = barcode[i] - '0';
        sum += (i % 2 == 0) ? d : (3 * d);
    }
    int check = (10 - (sum % 10)) % 10;
    if (!isdigit((unsigned char)barcode[12])) return false;
    return (check == (barcode[12] - '0'));
}

bool ep_validate(const EPData *in) {
    if (!in) return false;
    if (in->price < 0) return false;
    size_t len = strlen(in->barcode);
    if (len == 0 || len >= EP_BARCODE_MAX_LEN) return false;
    for (size_t i = 0; i < len; ++i) {
        if (!isdigit((unsigned char)in->barcode[i])) return false;
    }
    if (len == 13 && !ep_ean13_verify(in->barcode)) return false;
    return true;
}

EPStatus ep_parse(const char *json, EPData *out) {
    if (!json || !out) return EP_ERR_NULL;

    const char* v_add = find_key_value_start(json, "add");
    const char* v_price = find_key_value_start(json, "price");
    const char* v_barcode = find_key_value_start(json, "barcode");

    if (!v_add || !v_price || !v_barcode) return EP_ERR_KEY;

    int s;

    s = parse_add_auto(v_add, &out->add);
    if (s != EP_OK) return s;

    s = parse_int32(v_price, &out->price);
    if (s != EP_OK) return s;

    s = parse_string(v_barcode, out->barcode, sizeof(out->barcode));
    if (s != EP_OK) return s;

    if (!ep_validate(out)) return EP_ERR_VALUE;

    return EP_OK;
}

int ep_to_json(const EPData *in, char *buf, size_t buflen) {
    if (!in || !buf || buflen == 0) return EP_ERR_NULL;
    // Always emit add as hex uppercase with 4 digits.
    int n = snprintf(buf, buflen,
                     "{\"add\":\"0x%04X\",\"price\":%d,\"barcode\":\"%s\"}",
                     (unsigned)in->add, (int)in->price, in->barcode);
    if (n < 0) return EP_ERR_FORMAT;
    if ((size_t)n >= buflen) return EP_ERR_OVERFLOW;
    return n;
}


