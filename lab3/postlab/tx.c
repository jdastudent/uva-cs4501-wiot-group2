#include <stdio.h>
#include <zephyr/init.h>
#include <zephyr/kernel.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/logging/log.h>
#include <string.h>
#include "nrf_802154.h"

LOG_MODULE_REGISTER(send);

#define CHECK_PERIOD_MS  800u   
#define TX_IDLE_MS       7u     
#define FCS_LENGTH       2u
#define NUM_PACKETS      120 // if ~1.5ms to transmit, then 800ms check needs ~95 packets, 500ms check needs ~60

#define SW0_NODE DT_ALIAS(sw0)
static const struct gpio_dt_spec button = GPIO_DT_SPEC_GET(SW0_NODE, gpios);
static struct gpio_callback button_cb_data;

static volatile bool start_tx     = false;
static volatile bool ack_received = false;
static volatile bool tx_done      = false;
static uint8_t pkt[128];
static char text_payload[100];

static uint32_t stat_packets_sent     = 0;
static uint32_t stat_acks_received    = 0;
static uint32_t stat_bursts_triggered = 0;

static void button_pressed(const struct device *dev, struct gpio_callback *cb, uint32_t pins)
{
    if (!start_tx) {
        start_tx = true; ack_received = false; tx_done = false;
        // LOG_INF("Button pressed");
        stat_bursts_triggered++;
    }
}

void nrf_802154_tx_started(const uint8_t *p_frame) {}
void nrf_802154_transmit_failed(uint8_t *frame, nrf_802154_tx_error_t error, const nrf_802154_transmit_done_metadata_t *p_metadata) {}

void nrf_802154_transmitted_raw(uint8_t *p_frame, const nrf_802154_transmit_done_metadata_t *p_metadata)
{
    if (!tx_done && p_metadata->data.transmitted.p_ack != NULL) {
        if (!ack_received) {
            LOG_INF("ACK received - stopping train");
            ack_received = true;
            stat_acks_received++;
        }
        nrf_802154_buffer_free_raw(p_metadata->data.transmitted.p_ack);
    }
}

static int rf_setup(void) { nrf_802154_init(); return 0; }
SYS_INIT(rf_setup, POST_KERNEL, CONFIG_PTT_RF_INIT_PRIORITY);

int main(void)
{
    nrf_802154_channel_set(11u);
    int counter = 0;
    uint8_t pan_id[] = {0xcd, 0xab}, src_addr[] = {0xdc, 0xa9, 0x35, 0x7b, 0x73, 0x36, 0xce, 0xf4}, dst_addr[] = {0x50, 0xbe, 0xca, 0xc3, 0x3c, 0x36, 0xce, 0xf4};
    nrf_802154_pan_id_set(pan_id); nrf_802154_extended_address_set(src_addr);

    const nrf_802154_transmit_metadata_t metadata = { .frame_props = NRF_802154_TRANSMITTED_FRAME_PROPS_DEFAULT_INIT, .cca = true };
    gpio_pin_configure_dt(&button, GPIO_INPUT);
    gpio_pin_interrupt_configure_dt(&button, GPIO_INT_EDGE_TO_ACTIVE);
    gpio_init_callback(&button_cb_data, button_pressed, BIT(button.pin));
    gpio_add_callback(button.port, &button_cb_data);

    int64_t last_stats = k_uptime_get();

    while (1) {
        if (k_uptime_get() - last_stats >= 15000) {
            last_stats = k_uptime_get();

            LOG_INF("[Transmitter Stats] Bursts triggered: %u", stat_bursts_triggered);
            LOG_INF("[Transmitter Stats] Packets sent:     %u", stat_packets_sent);
            LOG_INF("[Transmitter Stats] ACKs received:    %u", stat_acks_received);
            LOG_INF("[Transmitter Stats] ACK miss rate:    %u%%", 
                    stat_bursts_triggered > 0 
                    ? (100 * (stat_bursts_triggered - stat_acks_received) / stat_bursts_triggered) 
                    : 0);
        }

        if (!start_tx) { k_sleep(K_MSEC(10)); continue; }

        for (uint32_t i = 0; i < NUM_PACKETS; i++) {
            if (ack_received) break;

            int text_len = snprintf(text_payload, sizeof(text_payload), 
                                    "Group2 Part %d", counter);
            if (text_len >= (int)sizeof(text_payload)) {
                LOG_ERR("Payload shortened");
                text_len = sizeof(text_payload) - 1;
            }
            
            if (i == 0) { LOG_INF("Sending: %s", text_payload); }

            uint8_t total_len = 23u + text_len + FCS_LENGTH;
            if (total_len > 127) {
                LOG_ERR("Packet too large (%u bytes)", total_len);
                break;
            }

            pkt[0] = total_len; 
            pkt[1] = 0x61; pkt[2] = 0xCC; pkt[3] = (uint8_t)counter;
            memcpy(&pkt[4], pan_id, 2); memcpy(&pkt[6], dst_addr, 8); 
            memcpy(&pkt[14], pan_id, 2); memcpy(&pkt[16], src_addr, 8); 
            memcpy(&pkt[24], text_payload, text_len);

            nrf_802154_transmit_raw(pkt, &metadata);
            stat_packets_sent++;
            k_sleep(K_MSEC(TX_IDLE_MS));
        }
        counter++; 
        start_tx = false; 
        nrf_802154_sleep();
    }
    return 0;
}
