#include <zephyr/types.h>
#include <stddef.h>
#include <errno.h>
#include <zephyr/kernel.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/bluetooth/bluetooth.h>
#include <zephyr/bluetooth/hci.h>
#include <zephyr/bluetooth/conn.h>
#include <zephyr/bluetooth/uuid.h>
#include <zephyr/bluetooth/gatt.h>

#define TARGET_SVC_UUID BT_UUID_128_ENCODE(0xBDFC9792, 0x8234, 0x405E, 0xAE02, 0x35EF4174B299)
static struct bt_uuid_128 uuid128 = BT_UUID_INIT_128(TARGET_SVC_UUID);

static struct bt_conn *default_conn;
static struct bt_gatt_discover_params disc_params;
static struct bt_gatt_subscribe_params sub_params[4];

static const struct gpio_dt_spec leds[] = {
    GPIO_DT_SPEC_GET(DT_ALIAS(led0), gpios),
    GPIO_DT_SPEC_GET(DT_ALIAS(led1), gpios),
    GPIO_DT_SPEC_GET(DT_ALIAS(led2), gpios),
    GPIO_DT_SPEC_GET(DT_ALIAS(led3), gpios),
};

static uint8_t on_notify(struct bt_conn *conn, struct bt_gatt_subscribe_params *params,
                         const void *data, uint16_t length)
{
    if (!data) {
        printk("Unsubscribed from handle %u\n", params->value_handle);
        return BT_GATT_ITER_STOP;
    }

    int led_idx = (int)(params - &sub_params[0]);
    uint8_t state = *(uint8_t *)data;

    printk("Notif: Button %d -> value %d\n", led_idx + 1, state);
    gpio_pin_set_dt(&leds[led_idx], state);

    return BT_GATT_ITER_CONTINUE;
}

// service -> char
static void discover_func(struct bt_conn *conn, const struct bt_gatt_attr *attr,
                          struct bt_gatt_discover_params *params)
{
    static int char_idx = 0;

    if (!attr) {
        printk("Everything discovered\n");
        return;
    }

    if (params->type == BT_GATT_DISCOVER_PRIMARY) {
        struct bt_gatt_service_val *gatt_svc = attr->user_data;
        printk("Service! Handles: 0x%04x to 0x%04x\n", 
                attr->handle, gatt_svc->end_handle);

        params->uuid = NULL; // find all chars
        params->start_handle = attr->handle + 1;
        params->end_handle = gatt_svc->end_handle;
        params->type = BT_GATT_DISCOVER_CHARACTERISTIC;
        
        bt_gatt_discover(conn, params);
        return;
    }

    if (params->type == BT_GATT_DISCOVER_CHARACTERISTIC) {
        struct bt_gatt_chrc *chrc = (struct bt_gatt_chrc *)attr->user_data;
        
        // 0x0001 to 0x0004
        if (chrc->uuid->type == BT_UUID_TYPE_16) {
            printk("Found Btn: 0x%04x\n", BT_UUID_16(chrc->uuid)->val);

            if (char_idx < 4) {
                sub_params[char_idx].notify = on_notify;
                sub_params[char_idx].value = BT_GATT_CCC_NOTIFY;
                sub_params[char_idx].value_handle = chrc->value_handle;
                sub_params[char_idx].ccc_handle = chrc->value_handle + 1;

                int err = bt_gatt_subscribe(conn, &sub_params[char_idx]);
                printk("Subscribed to Btn %d\n", char_idx + 1);
                char_idx++;
            }
        }
    }
}


static void connected(struct bt_conn *conn, uint8_t err) {
    printk("Connected\n");
    default_conn = bt_conn_ref(conn);

    disc_params.uuid = &uuid128.uuid; 
    disc_params.func = discover_func;
    disc_params.start_handle = BT_ATT_FIRST_ATTRIBUTE_HANDLE;
    disc_params.end_handle = BT_ATT_LAST_ATTRIBUTE_HANDLE;
    disc_params.type = BT_GATT_DISCOVER_PRIMARY; // to find service container

    bt_gatt_discover(default_conn, &disc_params);
}

static void disconnected(struct bt_conn *conn, uint8_t reason) {
    printk("Disconnected (reason %u)\n", reason);
    if (default_conn) {
        bt_conn_unref(default_conn);
        default_conn = NULL;
    }
    bt_le_scan_start(BT_LE_SCAN_PASSIVE, NULL); 
}

BT_CONN_CB_DEFINE(conn_callbacks) = {
    .connected = connected,
    .disconnected = disconnected,
};

static bool ad_has_target_uuid(struct net_buf_simple *ad)
{
    struct net_buf_simple dev_ad = *ad;
    
    while (dev_ad.len > 1) {
        uint8_t len = net_buf_simple_pull_u8(&dev_ad);
        if (len == 0) break;
        
        uint8_t type = net_buf_simple_pull_u8(&dev_ad);
        
        // 128bit service uuid
        if (type == BT_DATA_UUID128_ALL || type == BT_DATA_UUID128_SOME) {
            while (len > 1) {
                struct bt_uuid_128 ad_uuid;
                if (len < 16) break;
                
                uint8_t *uuid_val = net_buf_simple_pull_mem(&dev_ad, 16);
                bt_uuid_create(&ad_uuid.uuid, uuid_val, 16);

                if (bt_uuid_cmp(&ad_uuid.uuid, &uuid128.uuid) == 0) {
                    return true; // correct
                }
                len -= 16;
            }
        } else {
            // check if uuid list
            if (len - 1 <= dev_ad.len) {
                net_buf_simple_pull_mem(&dev_ad, len - 1);
            } else {
                break;
            }
        }
    }
    return false;
}

static void device_found(const bt_addr_le_t *addr, int8_t rssi, uint8_t type,
                         struct net_buf_simple *ad)
{
    // check address type
    if (type != BT_GAP_ADV_TYPE_ADV_IND && type != BT_GAP_ADV_TYPE_ADV_DIRECT_IND) {
        return;
    }

    // check service uuid
    if (!ad_has_target_uuid(ad)) {
        return; 
    }

    char addr_str[BT_ADDR_LE_STR_LEN];
    bt_addr_le_to_str(addr, addr_str, sizeof(addr_str));
    printk("Target found. %s (RSSI %d), will connect\n", addr_str, rssi);

    bt_le_scan_stop();
    int err = bt_conn_le_create(addr, BT_CONN_LE_CREATE_CONN, 
                                BT_LE_CONN_PARAM_DEFAULT, &default_conn);
    if (err) {
        printk("Connection failed (err %d)\n", err);
        bt_le_scan_start(BT_LE_SCAN_PASSIVE, device_found);
    }
}

int main(void) {
    printk("Starting LED board\n");
    
    int err = bt_enable(NULL);

    for (int i = 0; i < 4; i++) {
        gpio_pin_configure_dt(&leds[i], GPIO_OUTPUT_INACTIVE);
    }

    struct bt_le_scan_param scan_param = {
        .type = BT_LE_SCAN_TYPE_PASSIVE,
        .interval = BT_GAP_SCAN_FAST_INTERVAL,
        .window = BT_GAP_SCAN_FAST_WINDOW,
    };

    printk("Scanning for btn board\n");
    bt_le_scan_start(&scan_param, device_found);
    
    return 0;
}