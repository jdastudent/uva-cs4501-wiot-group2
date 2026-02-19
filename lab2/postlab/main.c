/**
 * Copyright (c) 2024 Croxel, Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <zephyr/types.h>
#include <stddef.h>
#include <zephyr/sys/printk.h>
#include <zephyr/sys/util.h>

#include <zephyr/bluetooth/bluetooth.h>
#include <zephyr/bluetooth/hci.h>

static void scan_recv(const struct bt_le_scan_recv_info *info, struct net_buf_simple *buf)
{
	if (info->adv_type == BT_GAP_ADV_TYPE_SCAN_RSP)
		return; // ignore scan responses as they are not ads

	char payload_str[128] = {0};
	for (size_t i = 0; i < buf->len && i < sizeof(payload_str) / 2; i++)
		snprintf(&payload_str[i * 2], 3, "%02X", buf->data[i]);
	
	printk("%u,", buf->len);
	printk("%s\n", payload_str);
}

static struct bt_le_scan_cb scan_callbacks = {
	.recv = scan_recv,
};

int main(void)
{
	/* Configuration of the Bluetooth Scanner */
	struct bt_le_scan_param scan_param = {
		.type       = BT_HCI_LE_SCAN_ACTIVE,
		.options    = BT_LE_SCAN_OPT_NONE,
		.interval   = 0x0080, // increased interval to reliably get packets
		.window     = 0x0080,
	};

	printf("len,payload\n");

	/* Initialize the Bluetooth Subsystem */
	int err = bt_enable(NULL);
	if (err) {
		printk("Bluetooth init failed (err %d)\n", err);
		return -1;
	}

	/* Register the scan callback function*/
	bt_le_scan_cb_register(&scan_callbacks);

	/* Start the Bluetooth scanner*/
	err = bt_le_scan_start(&scan_param, NULL);
	if (err) {
		printk("failed (err %d)\n", err);
		return -1;
	}
}
