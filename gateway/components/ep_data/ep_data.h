#ifndef EP_DATA_H
#define EP_DATA_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

// ============ Config ============
#ifndef EP_BARCODE_MAX_LEN
#define EP_BARCODE_MAX_LEN 64  // đủ dài cho EAN13/UPC/EAN8, v.v.
#endif

// ============ Status code ============
typedef enum {
    EP_OK = 0,
    EP_ERR_NULL     = -1,
    EP_ERR_KEY      = -2,
    EP_ERR_VALUE    = -3,
    EP_ERR_FORMAT   = -4,
    EP_ERR_OVERFLOW = -5
} EPStatus;

// ============ Data model ============
typedef struct {
    uint16_t add;                          // số lượng
    int32_t  price;                        // đơn giá (VND)
    char     barcode[EP_BARCODE_MAX_LEN];  // chỉ chữ số, NUL-terminated

    // Optional sale (percent 0..100)
    bool     has_sale;
    uint8_t  sale;                         // 0..100 (%)
} EPData;

// ============ API ============

// Parse JSON vào struct (yêu cầu có add, price, barcode; sale tùy chọn)
EPStatus ep_parse(const char *json, EPData *out);

// Serialize struct ra JSON. Trả về số byte đã ghi (không gồm NUL) hoặc <0 nếu lỗi.
// Lưu ý: add được in DEC (hệ 10) để khớp chuỗi đầu vào của bạn.
int ep_to_json(const EPData *in, char *buf, size_t buflen);

// Ràng buộc dữ liệu
bool ep_validate(const EPData *in);

// Tính giá sau giảm cho 1 sp (floor), nếu không có sale -> trả price
int32_t ep_unit_price_after_sale(const EPData *in);

// Tính thành tiền = unit_after_sale * add (dùng int64 tránh tràn)
int64_t ep_total_cost(const EPData *in);

// (Tuỳ chọn) Kiểm tra checksum EAN-13 khi barcode dài 13
bool ep_ean13_verify(const char *digits13);

#ifdef __cplusplus
}
#endif

#endif // EP_DATA_H
