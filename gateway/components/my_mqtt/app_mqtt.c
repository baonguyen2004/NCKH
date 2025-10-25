#include "app_mqtt.h"
#include <stdio.h>
#include <stdint.h>
#include <stddef.h>
#include <string.h>
#include <stdbool.h>
#include "esp_wifi.h"
#include "esp_system.h"
#include "nvs_flash.h"
#include "esp_event.h"
#include "esp_netif.h"

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"
#include "freertos/queue.h"

#include "lwip/sockets.h"
#include "lwip/dns.h"
#include "lwip/netdb.h"

#include "esp_log.h"
#include "mqtt_client.h"
#include "esp_err.h"

#include "mesh_vendor_api.h"


static const char *TAG = "mqtts_example";
static esp_mqtt_client_handle_t client ;
static volatile int s_has_data = 0;   // cờ dữ liệu sẵn sàng
static CmdMsg s_last;                 // bản ghi cuối

bool mqtt_try_get_last(CmdMsg *out) {
    if (!out) return false;
    if (!s_has_data) return false;
    // copy snapshot rồi clear flag
    *out = s_last;        // (nhỏ gọn; đủ tốt cho demo)
    s_has_data = 0;
    return true;
}

static void log_error_if_nonzero(const char *message, int error_code)
{
    if (error_code != 0) {
        ESP_LOGE(TAG, "Last error %s: 0x%x", message, error_code);
    }
}

static void mqtt_event_handler(void *handler_args, esp_event_base_t base, int32_t event_id, void *event_data)
{
    ESP_LOGD(TAG, "Event dispatched from event loop base=%s, event_id=%" PRIi32, base, event_id);
    esp_mqtt_event_handle_t event = event_data;
    esp_mqtt_client_handle_t client = event->client;
    int msg_id;
    switch ((esp_mqtt_event_id_t)event_id) {
    case MQTT_EVENT_CONNECTED:
        ESP_LOGI(TAG, "MQTT_EVENT_CONNECTED");
        msg_id = esp_mqtt_client_subscribe(client, "topic/command", 0);  //topic
        ESP_LOGI(TAG, "sent subscribe successful, msg_id=%d", msg_id);
        break;
    case MQTT_EVENT_DISCONNECTED:
        ESP_LOGI(TAG, "MQTT_EVENT_DISCONNECTED");
        break;

    case MQTT_EVENT_SUBSCRIBED:
        ESP_LOGI(TAG, "MQTT_EVENT_SUBSCRIBED, msg_id=%d", event->msg_id);
        break;
    case MQTT_EVENT_UNSUBSCRIBED:
        ESP_LOGI(TAG, "MQTT_EVENT_UNSUBSCRIBED, msg_id=%d", event->msg_id);
        break;
    case MQTT_EVENT_PUBLISHED:
        ESP_LOGI(TAG, "MQTT_EVENT_PUBLISHED, msg_id=%d", event->msg_id);
        break;
    case MQTT_EVENT_DATA: {
        ESP_LOGI(TAG, "MQTT_EVENT_DATA");
        printf("TOPIC=%.*s\r\n", event->topic_len, event->topic);
        printf("DATA=%.*s\r\n", event->data_len, event->data);

        // Copy payload sang buffer có NUL-terminator
        char json[256];
        size_t n = (size_t)event->data_len;
        if (n >= sizeof(json)) n = sizeof(json) - 1;
        memcpy(json, event->data, n);
        json[n] = '\0';

        EPData d;
        if (ep_parse(json, &d) == EP_OK) {
            // ---- GÁN ĐẦY ĐỦ VÀO s_last để bên BLE Mesh lấy ra ----
            s_last.add = d.add;
            s_last.price = d.price;

            // copy barcode an toàn, luôn NUL-terminate
            strncpy(s_last.barcode, d.barcode, sizeof(s_last.barcode));
            s_last.barcode[sizeof(s_last.barcode) - 1] = '\0';

            // NEW: copy sale (%)
            s_last.has_sale = d.has_sale;
            s_last.sale     = d.sale;  // 0..100 nếu has_sale=true

            // (nếu bạn đang dùng API dạng kho chung)
            // mqtt_set_last(&s_last);    // <-- dùng khi có hàm này

            // ---- Log rõ ràng ----
            int32_t unit_after = ep_unit_price_after_sale(&d);   // = price nếu không có sale
            int64_t total      = ep_total_cost(&d);

            if (d.has_sale) {
                ESP_LOGI(TAG,
                    "Parsed OK: add=%u price=%d barcode=%s sale=%u%% -> unit=%d, total=%lld",
                    (unsigned)d.add, (int)d.price, d.barcode,
                    (unsigned)d.sale, (int)unit_after, (long long)total);
            } else {
                ESP_LOGI(TAG,
                    "Parsed OK: add=%u price=%d barcode=%s sale=NA -> unit=%d, total=%lld",
                    (unsigned)d.add, (int)d.price, d.barcode,
                    (int)unit_after, (long long)total);
            }

            // Đặt cờ có dữ liệu mới (nếu bạn đang dùng cơ chế try_get_last)
            s_has_data = 1;

            // Gửi luôn (nếu bạn muốn bắn ngay sau khi nhận MQTT)
            example_ble_mesh_send_vendor_message(false);

        } else {
            ESP_LOGE(TAG, "Parse fail (payload khong dung format)");
        }
        break;
    }
    case MQTT_EVENT_ERROR:
        ESP_LOGI(TAG, "MQTT_EVENT_ERROR");
        if (event->error_handle->error_type == MQTT_ERROR_TYPE_TCP_TRANSPORT) {
            log_error_if_nonzero("reported from esp-tls", event->error_handle->esp_tls_last_esp_err);
            log_error_if_nonzero("reported from tls stack", event->error_handle->esp_tls_stack_err);
            log_error_if_nonzero("captured as transport's socket errno",  event->error_handle->esp_transport_sock_errno);
            ESP_LOGI(TAG, "Last errno string (%s)", strerror(event->error_handle->esp_transport_sock_errno));

        }
        break;
    default:
        ESP_LOGI(TAG, "Other event id:%d", event->event_id);
        break;
    }
}

void mqtt_app_start(void)
{
    const esp_mqtt_client_config_t mqtt_cfg = {
        .broker.address.uri = "mqtt://broker.hivemq.com:1883",
    };

    ESP_LOGI(TAG, "[APP] Free memory: %" PRIu32 " bytes", esp_get_free_heap_size());
    client = esp_mqtt_client_init(&mqtt_cfg);   
    /* The last argument may be used to pass data to the event handler, in this example mqtt_event_handler */
    esp_mqtt_client_register_event(client, ESP_EVENT_ANY_ID, mqtt_event_handler, NULL);
    esp_mqtt_client_start(client);
}