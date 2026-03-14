/*
 * Copyright (c) 2021 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: LicenseRef-Nordic-5-Clause
 */
#include <stdio.h>
#include <zephyr/init.h>
#include <zephyr/kernel.h>

#include <zephyr/logging/log.h>
	LOG_MODULE_REGISTER(recv);

#include "nrf_802154.h"

#define PSDU_MAX_SIZE (127u)
#define CHECK_PERIOD_TICKS 800 // in ms
#define RX_WINDOW_TICKS 10 // > 8.2 ms = 2 * the duration of a transmission of the longest 802.15.4 packet
#define STATS_PERIOD_TICKS 15000 // 15 seconds

static volatile bool pkt_recvd = false;
static volatile bool rx_finished = false;

static void pkt_hexdump(uint8_t *pkt, uint8_t length) {
  int i;
  printk("Packet: ");
	for (i=0; i<length; i++) {
		printk("%02x ", *pkt);
		pkt++;
	}
	printk("\n");
}

static int rf_setup()
{
	LOG_INF("RF setup started");
	printk("RF setup started\n");

	/* nrf radio driver initialization */
	nrf_802154_init();
	return 0;
}

void nrf_802154_received_raw(uint8_t *data, int8_t power, uint8_t lqi) {
	if (!rx_finished) {
		pkt_hexdump(data+1, *data - 2U); /* print packet from byte [1, len-2] */
		pkt_recvd = true;
		rx_finished = true;
	}
	nrf_802154_buffer_free_raw(data);
}

void nrf_802154_receive_failed(nrf_802154_rx_error_t error, uint32_t id) {
 	LOG_INF("%d: rx failed error %u!", id, error);
}

int main(void) {
	nrf_802154_channel_set(11u);
	nrf_802154_auto_ack_set(true); // auto ACK
	LOG_DBG("channel: %u", nrf_802154_channel_get());
	printf("channel: %u\n", nrf_802154_channel_get());

	// set the pan_id (2 bytes, little-endian)
	uint8_t pan_id[] = {0xcd, 0xab};
	nrf_802154_pan_id_set(pan_id);

	// set the extended address (8 bytes, little-endian)
	uint8_t extended_addr[] = {0x50, 0xbe, 0xca, 0xc3, 0x3c, 0x36, 0xce, 0xf4};
	nrf_802154_extended_address_set(extended_addr);

	uint64_t last_stats = k_uptime_ticks();
	while (true) {
		k_sleep(K_TICKS(CHECK_PERIOD_TICKS));
		pkt_recvd = false;
		rx_finished = false;

		nrf_802154_receive();
		uint64_t start = k_uptime_ticks();

		while (!rx_finished) {
			uint64_t elapsed = k_uptime_ticks() - start;
			if (elapsed >= RX_WINDOW_TICKS)
				rx_finished = true;
		}

		nrf_802154_sleep();
		uint64_t now = k_uptime_ticks();

		if (now - last_stats >= STATS_PERIOD_TICKS) {
			nrf_802154_stats_t stats;
        	nrf_802154_stats_get(&stats);

        	LOG_INF("[Receiver Stats] CCA fails: %llu", stats.counters.cca_failed_attempts);
        	LOG_INF("[Receiver Stats] RX Events: %llu", stats.counters.received_energy_events);
        	LOG_INF("[Receiver Stats] Frames received: %llu", stats.counters.received_frames);

        	last_stats = now;
		}
	}

	return 0;
}

SYS_INIT(rf_setup, POST_KERNEL, CONFIG_PTT_RF_INIT_PRIORITY);
