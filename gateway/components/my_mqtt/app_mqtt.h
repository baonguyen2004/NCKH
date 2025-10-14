#ifndef __APP_MQTT_H
#define __APP_MQTT_H

#include <stdint.h>
#include "ep_data.h"

void mqtt_app_start(void);

typedef struct {
    uint16_t add;
    int32_t  price;
    char     barcode[EP_BARCODE_MAX_LEN];
} CmdMsg;

// lấy bản ghi mới nhất (trả true nếu có dữ liệu)
bool mqtt_try_get_last(CmdMsg *out);

#endif
