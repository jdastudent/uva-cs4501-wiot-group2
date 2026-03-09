#include <zephyr/kernel.h>
#include <zephyr/bluetooth/bluetooth.h>
#include <zephyr/bluetooth/gatt.h>
#include <zephyr/drivers/gpio.h>

#define SVC_UUID BT_UUID_128_ENCODE(0xBDFC9792, 0x8234, 0x405E, 0xAE02, 0x35EF4174B299)

static const struct gpio_dt_spec buttons[] = {
    GPIO_DT_SPEC_GET(DT_ALIAS(sw0), gpios),
    GPIO_DT_SPEC_GET(DT_ALIAS(sw1), gpios),
    GPIO_DT_SPEC_GET(DT_ALIAS(sw2), gpios),
    GPIO_DT_SPEC_GET(DT_ALIAS(sw3), gpios),
};

static uint8_t btn_states[4] = {0};

static void ccc_cfg_changed(const struct bt_gatt_attr *attr, uint16_t value) {
    bool notif_enabled = (value == BT_GATT_CCC_NOTIFY);
    printk("Notifs %s\n", notif_enabled ? "enabled" : "disabled");
}

BT_GATT_SERVICE_DEFINE(button_svc,
    BT_GATT_PRIMARY_SERVICE(BT_UUID_DECLARE_128(SVC_UUID)),
    
    BT_GATT_CHARACTERISTIC(BT_UUID_DECLARE_16(0x0001), BT_GATT_CHRC_NOTIFY, BT_GATT_PERM_NONE, NULL, NULL, NULL),
    BT_GATT_CCC(ccc_cfg_changed, BT_GATT_PERM_READ | BT_GATT_PERM_WRITE),

    BT_GATT_CHARACTERISTIC(BT_UUID_DECLARE_16(0x0002), BT_GATT_CHRC_NOTIFY, BT_GATT_PERM_NONE, NULL, NULL, NULL),
    BT_GATT_CCC(ccc_cfg_changed, BT_GATT_PERM_READ | BT_GATT_PERM_WRITE),

    BT_GATT_CHARACTERISTIC(BT_UUID_DECLARE_16(0x0003), BT_GATT_CHRC_NOTIFY, BT_GATT_PERM_NONE, NULL, NULL, NULL),
    BT_GATT_CCC(ccc_cfg_changed, BT_GATT_PERM_READ | BT_GATT_PERM_WRITE),

    BT_GATT_CHARACTERISTIC(BT_UUID_DECLARE_16(0x0004), BT_GATT_CHRC_NOTIFY, BT_GATT_PERM_NONE, NULL, NULL, NULL),
    BT_GATT_CCC(ccc_cfg_changed, BT_GATT_PERM_READ | BT_GATT_PERM_WRITE),
);

void button_pressed(const struct device *dev, struct gpio_callback *cb, uint32_t pins) {
    for (int i = 0; i < 4; i++) {
        if (pins & BIT(buttons[i].pin)) {
            btn_states[i] = gpio_pin_get_dt(&buttons[i]);
            // 2, 5, 8, & 11 are the char value attribs
            struct bt_uuid_16 target_uuid = BT_UUID_INIT_16(0x0001 + i);
            const struct bt_gatt_attr *attr = bt_gatt_find_by_uuid(button_svc.attrs, button_svc.attr_count, &target_uuid.uuid);

            if (attr) {
                bt_gatt_notify(NULL, attr + 1, &btn_states[i], 1); // attr is metadata, after is val
                
                printk("Notified button %d: state %d (handle 0x%04x)\n", 
                        i + 1, btn_states[i], attr->handle);
            }
        }
    }
}

static struct gpio_callback btn_cb_data;

int main(void) {
    bt_enable(NULL);
    
    for (int i = 0; i < 4; i++) {
        gpio_pin_configure_dt(&buttons[i], GPIO_INPUT);
        gpio_pin_interrupt_configure_dt(&buttons[i], GPIO_INT_EDGE_BOTH);
    }
    gpio_init_callback(
        &btn_cb_data,
        button_pressed,
        BIT(buttons[0].pin) | BIT(buttons[1].pin) | BIT(buttons[2].pin) | BIT(buttons[3].pin)
    );
    for (int i = 0; i < 4; i++) gpio_add_callback(buttons[i].port, &btn_cb_data);

    static const struct bt_data ad[] = {
        BT_DATA_BYTES(BT_DATA_FLAGS, (BT_LE_AD_GENERAL | BT_LE_AD_NO_BREDR)),
        BT_DATA_BYTES(BT_DATA_UUID128_ALL, SVC_UUID),
    };

    bt_le_adv_start(BT_LE_ADV_CONN, ad, ARRAY_SIZE(ad), NULL, 0);
    return 0;
}