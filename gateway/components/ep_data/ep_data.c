#include "ep_data.h"
#include <string.h>
#include <ctype.h>
#include <stdio.h>

// ======== small helpers ========
static const char* skip_ws(const char* s) {
    while (s && *s && isspace((unsigned char)*s)) ++s;
    return s;
}

static const char* find_key_value_start(const char* json, const char* key) {
    if (!json || !key) return NULL;
    // tìm chuỗi: "key"
    const size_t klen = strlen(key);
    const char *p = json;
    while ((p = strstr(p, "\"")) != NULL) {
        ++p;
        if (strncmp(p, key, klen) == 0 && p[klen] == '\"') {
            p += klen + 1;
            p = skip_ws(p);
            if (*p != ':') continue;
            ++p; // qua ':'
            p = skip_ws(p);
            return p; // trỏ vào ký tự đầu tiên của value
        }
    }
    return NULL;
}

static EPStatus parse_uint16(const char* v, uint16_t* out) {
    if (!v || !out) return EP_ERR_NULL;
    // number DEC hoặc HEX "0x.."
    const char* p = v;
    int base = 10;
    if (p[0]=='0' && (p[1]=='x' || p[1]=='X')) {
        base = 16; p += 2;
    }
    unsigned long val = 0;
    for (; *p; ++p) {
        if (base==16 ? isxdigit((unsigned char)*p) : isdigit((unsigned char)*p)) {
            int d;
            if (isdigit((unsigned char)*p)) d = *p - '0';
            else d = 10 + (tolower((unsigned char)*p) - 'a');
            val = (base==16) ? (val*16 + (unsigned)d) : (val*10 + (unsigned)d);
            if (val > 0xFFFFUL) return EP_ERR_OVERFLOW;
        } else break;
    }
    *out = (uint16_t)val;
    return EP_OK;
}

static EPStatus parse_int32(const char* v, int32_t* out) {
    if (!v || !out) return EP_ERR_NULL;
    const char* p = v;
    int sign = 1;
    if (*p=='+' || *p=='-') { if (*p=='-') sign=-1; ++p; }
    long long val = 0;
    if (!isdigit((unsigned char)*p)) return EP_ERR_FORMAT;
    for (; *p; ++p) {
        if (!isdigit((unsigned char)*p)) break;
        val = val*10 + (*p - '0');
        if (val > 2147483647LL) return EP_ERR_OVERFLOW;
    }
    *out = (int32_t)(sign * val);
    return EP_OK;
}

static EPStatus parse_string(const char* v, char* buf, size_t buflen) {
    if (!v || !buf || buflen==0) return EP_ERR_NULL;
    const char* p = skip_ws(v);
    if (*p != '\"') return EP_ERR_FORMAT;
    ++p;
    size_t i = 0;
    while (*p && *p != '\"') {
        if (i+1 >= buflen) return EP_ERR_OVERFLOW;
        // đơn giản: không unescape, giả định dữ liệu sạch
        buf[i++] = *p++;
    }
    if (*p != '\"') return EP_ERR_FORMAT;
    buf[i] = '\0';
    return EP_OK;
}

static EPStatus parse_percent_0_100(const char* v, uint8_t* out) {
    if (!v || !out) return EP_ERR_NULL;
    int32_t tmp = 0;
    EPStatus s = parse_int32(v, &tmp);
    if (s != EP_OK) return s;
    if (tmp < 0 || tmp > 100) return EP_ERR_VALUE;
    *out = (uint8_t)tmp;
    return EP_OK;
}

static bool is_all_digits(const char* s) {
    if (!s || !*s) return false;
    for (const char* p=s; *p; ++p) {
        if (!isdigit((unsigned char)*p)) return false;
    }
    return true;
}

// ======== public ========

bool ep_validate(const EPData *in) {
    if (!in) return false;

    // price hợp lệ
    if (in->price < 0) return false;

    // barcode: chỉ chữ số, độ dài hợp lệ
    size_t len = strlen(in->barcode);
    if (len == 0 || len >= EP_BARCODE_MAX_LEN) return false;
    if (!is_all_digits(in->barcode)) return false;

    // Nếu dài 13 -> kiểm tra checksum EAN-13
    if (len == 13 && !ep_ean13_verify(in->barcode)) return false;

    // sale (nếu có)
    if (in->has_sale && in->sale > 100) return false;

    return true;
}

EPStatus ep_parse(const char *json, EPData *out) {
    if (!json || !out) return EP_ERR_NULL;

    const char* v_add     = find_key_value_start(json, "add");
    const char* v_price   = find_key_value_start(json, "price");
    const char* v_barcode = find_key_value_start(json, "barcode");
    const char* v_sale    = find_key_value_start(json, "sale");

    if (!v_add || !v_price || !v_barcode) return EP_ERR_KEY;

    EPStatus s;

    s = parse_uint16(v_add, &out->add);
    if (s != EP_OK) return s;

    s = parse_int32(v_price, &out->price);
    if (s != EP_OK) return s;

    s = parse_string(v_barcode, out->barcode, sizeof(out->barcode));
    if (s != EP_OK) return s;

    out->has_sale = false;
    out->sale = 0;
    if (v_sale) {
        uint8_t p = 0;
        s = parse_percent_0_100(v_sale, &p);
        if (s != EP_OK) return s;
        out->has_sale = true;
        out->sale = p;
    }

    if (!ep_validate(out)) return EP_ERR_VALUE;

    return EP_OK;
}

int ep_to_json(const EPData *in, char *buf, size_t buflen) {
    if (!in || !buf || buflen==0) return EP_ERR_NULL;
    // output DEC cho add để khớp thói quen nhập
    int n;
    if (in->has_sale) {
        n = snprintf(buf, buflen,
            "{\"add\":%u,\"price\":%d,\"barcode\":\"%s\",\"sale\":%u}",
            (unsigned)in->add, (int)in->price, in->barcode, (unsigned)in->sale);
    } else {
        n = snprintf(buf, buflen,
            "{\"add\":%u,\"price\":%d,\"barcode\":\"%s\"}",
            (unsigned)in->add, (int)in->price, in->barcode);
    }
    if (n < 0) return EP_ERR_FORMAT;
    if ((size_t)n >= buflen) return EP_ERR_OVERFLOW;
    return n; // số byte đã ghi (không tính NUL)
}

int32_t ep_unit_price_after_sale(const EPData *in) {
    if (!in) return 0;
    if (!in->has_sale) return in->price;
    // floor(price * (100 - sale) / 100)
    int64_t num = (int64_t)in->price * (int64_t)(100 - in->sale);
    return (int32_t)(num / 100);
}

int64_t ep_total_cost(const EPData *in) {
    if (!in) return 0;
    int32_t unit = ep_unit_price_after_sale(in);
    return (int64_t)unit * (int64_t)in->add;
}

// EAN-13 checksum:
// Tính tổng: (tổng chữ số ở vị trí lẻ từ phải tính 1) * 3 + (tổng chữ số ở vị trí chẵn)
// check_digit = (10 - (sum % 10)) % 10
bool ep_ean13_verify(const char *digits13) {
    if (!digits13) return false;
    size_t len = strlen(digits13);
    if (len != 13) return false;
    if (!is_all_digits(digits13)) return false;

    int sum = 0;
    for (int i = 0; i < 12; ++i) {
        int d = digits13[i] - '0';
        // từ trái sang phải: vị trí (i) -> tương ứng với (12 - i) từ phải
        int pos_from_right = 12 - i; // 12..1
        if ((pos_from_right % 2) == 1) { // lẻ
            sum += d * 3;
        } else {
            sum += d;
        }
    }
    int check = (10 - (sum % 10)) % 10;
    return check == (digits13[12] - '0');
}
