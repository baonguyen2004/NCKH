/* main.c - Application main entry point */
/*
 * SPDX-FileCopyrightText: 2017 Intel Corporation
 * SPDX-FileContributor: 2018-2021 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <stdio.h>
#include <string.h>
#include <inttypes.h>

#include "esp_log.h"
#include "nvs_flash.h"
#include "esp_bt.h"

#include "esp_ble_mesh_defs.h"
#include "esp_ble_mesh_common_api.h"
#include "esp_ble_mesh_networking_api.h"
#include "esp_ble_mesh_provisioning_api.h"
#include "esp_ble_mesh_config_model_api.h"
#include "esp_ble_mesh_local_data_operation_api.h"

#include "ble_mesh_example_init.h"

#define TAG "EXAMPLE"

#define CID_ESP     0x02E5  /* Company ID for vendor model */

#define ESP_BLE_MESH_VND_MODEL_ID_CLIENT    0x0000
#define ESP_BLE_MESH_VND_MODEL_ID_SERVER    0x0001

#define ESP_BLE_MESH_VND_MODEL_OP_SEND      ESP_BLE_MESH_MODEL_OP_3(0x00, CID_ESP)
#define ESP_BLE_MESH_VND_MODEL_OP_STATUS    ESP_BLE_MESH_MODEL_OP_3(0x01, CID_ESP)

/* Node UUID: 0x32 0x10 ... để provisioner lọc */
static uint8_t dev_uuid[ESP_BLE_MESH_OCTET16_LEN] = {
    0x32, 0x10, 0,0, 0,0, 0,0, 0,0, 0,0, 0,0
};

static uint16_t g_my_primary_addr = 0x0000;

/* Gói sau khi parse (mục đích in log) */
typedef struct {
    uint16_t tid;          /* LE */
    uint32_t price;        /* LE */
    char     ean13[14];    /* 13 digits + '\0' */
} rx_packet_t;

/* Chuyển 7 byte BCD -> 13 ký tự số */
static void bcd7_to_ean13(const uint8_t bcd[7], char out[14]) {
    for (int i = 0, j = 0; i < 6; ++i) {
        out[j++] = '0' + ((bcd[i] >> 4) & 0x0F);
        out[j++] = '0' + (bcd[i] & 0x0F);
    }
    out[12] = '0' + ((bcd[6] >> 4) & 0x0F);
    out[13] = '\0';
}

/* Parse đúng format 13B: TID(2) + PRICE(4 LE) + EAN13_BCD(7) */
static bool parse_vendor_payload(const uint8_t *msg, uint16_t len, rx_packet_t *out) {
    if (!msg || !out || len < 13) return false;
    out->tid   = (uint16_t)(msg[0] | (msg[1] << 8));
    out->price =  (uint32_t)msg[2]
                | ((uint32_t)msg[3] << 8)
                | ((uint32_t)msg[4] << 16)
                | ((uint32_t)msg[5] << 24);
    bcd7_to_ean13(&msg[6], out->ean13);
    return true;
}

/* ----- Model / Composition ----- */

static esp_ble_mesh_cfg_srv_t config_server = {
    .net_transmit = ESP_BLE_MESH_TRANSMIT(2, 20),
    .relay = ESP_BLE_MESH_RELAY_DISABLED,
    .relay_retransmit = ESP_BLE_MESH_TRANSMIT(2, 20),
    .beacon = ESP_BLE_MESH_BEACON_ENABLED,
#if defined(CONFIG_BLE_MESH_GATT_PROXY_SERVER)
    .gatt_proxy = ESP_BLE_MESH_GATT_PROXY_ENABLED,
#else
    .gatt_proxy = ESP_BLE_MESH_GATT_PROXY_NOT_SUPPORTED,
#endif
#if defined(CONFIG_BLE_MESH_FRIEND)
    .friend_state = ESP_BLE_MESH_FRIEND_ENABLED,
#else
    .friend_state = ESP_BLE_MESH_FRIEND_NOT_SUPPORTED,
#endif
    .default_ttl = 7,
};

static esp_ble_mesh_model_t root_models[] = {
    ESP_BLE_MESH_MODEL_CFG_SRV(&config_server),
};

/* Giữ min-len = 13 để khớp client mới */
static esp_ble_mesh_model_op_t vnd_op[] = {
    ESP_BLE_MESH_MODEL_OP(ESP_BLE_MESH_VND_MODEL_OP_SEND, 13),
    ESP_BLE_MESH_MODEL_OP_END,
};

static esp_ble_mesh_model_t vnd_models[] = {
    ESP_BLE_MESH_VENDOR_MODEL(CID_ESP, ESP_BLE_MESH_VND_MODEL_ID_SERVER,
                              vnd_op, NULL, NULL),
};

static esp_ble_mesh_elem_t elements[] = {
    ESP_BLE_MESH_ELEMENT(0, root_models, vnd_models),
};

static esp_ble_mesh_comp_t composition = {
    .cid = CID_ESP,
    .element_count = ARRAY_SIZE(elements),
    .elements = elements,
};

static esp_ble_mesh_prov_t provision = {
    .uuid = dev_uuid,
};

/* ----- Provisioning callbacks ----- */

static void prov_complete(uint16_t net_idx, uint16_t addr, uint8_t flags, uint32_t iv_index)
{
    ESP_LOGI(TAG, "net_idx 0x%03x, addr 0x%04x", net_idx, addr);
    ESP_LOGI(TAG, "flags 0x%02x, iv_index 0x%08" PRIx32, flags, iv_index);
    g_my_primary_addr = addr;
    ESP_LOGI(TAG, "=== MY UNICAST ADDR: 0x%04x ===", g_my_primary_addr);
}

static void example_ble_mesh_provisioning_cb(esp_ble_mesh_prov_cb_event_t event,
                                             esp_ble_mesh_prov_cb_param_t *param)
{
    switch (event) {
    case ESP_BLE_MESH_PROV_REGISTER_COMP_EVT:
        ESP_LOGI(TAG, "ESP_BLE_MESH_PROV_REGISTER_COMP_EVT, err_code %d", param->prov_register_comp.err_code);
        break;
    case ESP_BLE_MESH_NODE_PROV_ENABLE_COMP_EVT:
        ESP_LOGI(TAG, "ESP_BLE_MESH_NODE_PROV_ENABLE_COMP_EVT, err_code %d", param->node_prov_enable_comp.err_code);
        break;
    case ESP_BLE_MESH_NODE_PROV_LINK_OPEN_EVT:
        ESP_LOGI(TAG, "ESP_BLE_MESH_NODE_PROV_LINK_OPEN_EVT, bearer %s",
                 param->node_prov_link_open.bearer == ESP_BLE_MESH_PROV_ADV ? "PB-ADV" : "PB-GATT");
        break;
    case ESP_BLE_MESH_NODE_PROV_LINK_CLOSE_EVT:
        ESP_LOGI(TAG, "ESP_BLE_MESH_NODE_PROV_LINK_CLOSE_EVT, bearer %s",
                 param->node_prov_link_close.bearer == ESP_BLE_MESH_PROV_ADV ? "PB-ADV" : "PB-GATT");
        break;
    case ESP_BLE_MESH_NODE_PROV_COMPLETE_EVT:
        ESP_LOGI(TAG, "ESP_BLE_MESH_NODE_PROV_COMPLETE_EVT");
        prov_complete(param->node_prov_complete.net_idx, param->node_prov_complete.addr,
                      param->node_prov_complete.flags, param->node_prov_complete.iv_index);
        break;
    case ESP_BLE_MESH_NODE_PROV_RESET_EVT:
        ESP_LOGI(TAG, "ESP_BLE_MESH_NODE_PROV_RESET_EVT");
        break;
    case ESP_BLE_MESH_NODE_SET_UNPROV_DEV_NAME_COMP_EVT:
        ESP_LOGI(TAG, "ESP_BLE_MESH_NODE_SET_UNPROV_DEV_NAME_COMP_EVT, err_code %d",
                 param->node_set_unprov_dev_name_comp.err_code);
        break;
    default:
        break;
    }
}

/* ----- Config server callback ----- */

static void example_ble_mesh_config_server_cb(esp_ble_mesh_cfg_server_cb_event_t event,
                                              esp_ble_mesh_cfg_server_cb_param_t *param)
{
    if (event == ESP_BLE_MESH_CFG_SERVER_STATE_CHANGE_EVT) {
        switch (param->ctx.recv_op) {
        case ESP_BLE_MESH_MODEL_OP_APP_KEY_ADD:
            ESP_LOGI(TAG, "ESP_BLE_MESH_MODEL_OP_APP_KEY_ADD");
            ESP_LOGI(TAG, "net_idx 0x%04x, app_idx 0x%04x",
                     param->value.state_change.appkey_add.net_idx,
                     param->value.state_change.appkey_add.app_idx);
            ESP_LOG_BUFFER_HEX("AppKey", param->value.state_change.appkey_add.app_key, 16);
            break;
        case ESP_BLE_MESH_MODEL_OP_MODEL_APP_BIND:
            ESP_LOGI(TAG, "ESP_BLE_MESH_MODEL_OP_MODEL_APP_BIND");
            ESP_LOGI(TAG, "elem_addr 0x%04x, app_idx 0x%04x, cid 0x%04x, mod_id 0x%04x",
                     param->value.state_change.mod_app_bind.element_addr,
                     param->value.state_change.mod_app_bind.app_idx,
                     param->value.state_change.mod_app_bind.company_id,
                     param->value.state_change.mod_app_bind.model_id);
            break;
        default:
            break;
        }
    }
}

/* ----- Vendor model callback (RECV & ACK) ----- */

static void example_ble_mesh_custom_model_cb(esp_ble_mesh_model_cb_event_t event,
                                             esp_ble_mesh_model_cb_param_t *param)
{
    switch (event) {
    case ESP_BLE_MESH_MODEL_OPERATION_EVT:
        if (param->model_operation.opcode == ESP_BLE_MESH_VND_MODEL_OP_SEND) {
            const uint8_t *msg = param->model_operation.msg;
            uint16_t       len = param->model_operation.length;

            rx_packet_t rx;

            if (len >= 13 && parse_vendor_payload(msg, len, &rx)) {
                uint16_t src = param->model_operation.ctx->addr;
                ESP_LOGI(TAG,
                         "Recv 0x%06" PRIx32 " from 0x%04x | TID=0x%04x | price=%" PRIu32 " | barcode=%s",
                         (unsigned long)param->model_operation.opcode,  /* cast để khỏi cảnh báo format */
                         src, rx.tid, rx.price, rx.ean13);

                /* ACK theo đúng TID để client không timeout */
                esp_err_t err = esp_ble_mesh_server_model_send_msg(
                                    &vnd_models[0], param->model_operation.ctx,
                                    ESP_BLE_MESH_VND_MODEL_OP_STATUS,
                                    sizeof(rx.tid), (uint8_t *)&rx.tid);
                if (err) {
                    ESP_LOGE(TAG, "Failed to send STATUS (err 0x%x)", err);
                } else {
                    ESP_LOGI(TAG, "ACKed TID 0x%04x", rx.tid);
                }
            } else {
                /* Payload thiếu: cố đọc TID để vẫn ACK, giúp client không timeout */
                ESP_LOGE(TAG, "Bad payload (len=%u), need >=13 bytes", len);

                if (len >= 2) {
                    uint16_t tid = (uint16_t)(msg[0] | (msg[1] << 8));
                    esp_err_t err = esp_ble_mesh_server_model_send_msg(
                                        &vnd_models[0], param->model_operation.ctx,
                                        ESP_BLE_MESH_VND_MODEL_OP_STATUS,
                                        sizeof(tid), (uint8_t *)&tid);
                    if (err) {
                        ESP_LOGE(TAG, "STATUS for bad payload failed (err 0x%x)", err);
                    } else {
                        ESP_LOGW(TAG, "ACKed (bad payload) TID 0x%04x", tid);
                    }
                }
            }
        }
        break;

    case ESP_BLE_MESH_MODEL_SEND_COMP_EVT:
        if (param->model_send_comp.err_code) {
            ESP_LOGE(TAG, "Failed to send message 0x%06" PRIx32,
                     (unsigned long)param->model_send_comp.opcode);
            break;
        }
        ESP_LOGI(TAG, "Send 0x%06" PRIx32,
                 (unsigned long)param->model_send_comp.opcode);
        break;

    default:
        break;
    }
}

/* ----- Init & app_main ----- */

static esp_err_t ble_mesh_init(void)
{
    esp_err_t err;

    esp_ble_mesh_register_prov_callback(example_ble_mesh_provisioning_cb);
    esp_ble_mesh_register_config_server_callback(example_ble_mesh_config_server_cb);
    esp_ble_mesh_register_custom_model_callback(example_ble_mesh_custom_model_cb);

    err = esp_ble_mesh_init(&provision, &composition);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to initialize mesh stack");
        return err;
    }

    err = esp_ble_mesh_node_prov_enable((esp_ble_mesh_prov_bearer_t)
            (ESP_BLE_MESH_PROV_ADV | ESP_BLE_MESH_PROV_GATT));
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to enable mesh node");
        return err;
    }

    ESP_LOGI(TAG, "BLE Mesh Node initialized");
    return ESP_OK;
}

void app_main(void)
{
    esp_err_t err;

    ESP_LOGI(TAG, "Initializing...");

    err = nvs_flash_init();
    if (err == ESP_ERR_NVS_NO_FREE_PAGES) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        err = nvs_flash_init();
    }
    ESP_ERROR_CHECK(err);

    err = bluetooth_init();
    if (err) {
        ESP_LOGE(TAG, "esp32_bluetooth_init failed (err %d)", err);
        return;
    }

    err = ble_mesh_init();
    if (err) {
        ESP_LOGE(TAG, "Bluetooth mesh init failed (err %d)", err);
    }
}
