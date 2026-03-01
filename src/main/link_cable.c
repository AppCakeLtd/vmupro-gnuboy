/*
 * Wireless Link Cable implementation.
 *
 * Milestone 2: Discovery, pairing, keepalive.
 * Milestone 3: Serial data exchange (master/slave transfer functions).
 */

#include "link_cable.h"
#include <vmupro_peernet.h>
#include <vmupro_sdk.h>
#include <string.h>

/* ------------------------------------------------------------------ */
/* Wire protocol                                                       */
/* ------------------------------------------------------------------ */

#define LINK_PKT_DISCOVER      0x01
#define LINK_PKT_DISCOVER_ACK  0x02
#define LINK_PKT_SERIAL_DATA   0x03
#define LINK_PKT_SERIAL_ACK    0x04
#define LINK_PKT_PING          0x05
#define LINK_PKT_DISCONNECT    0x06
#define LINK_PKT_PAUSE         0x07

/* Magic bytes to identify our packets (avoid collisions with other apps) */
#define LINK_MAGIC_0  0x47  /* 'G' */
#define LINK_MAGIC_1  0x42  /* 'B' */

typedef struct __attribute__((packed)) {
    uint8_t  magic[2];   /* LINK_MAGIC_0, LINK_MAGIC_1 */
    uint8_t  type;       /* LINK_PKT_* */
    uint8_t  payload;    /* Data byte (for SERIAL_DATA/SERIAL_ACK) */
    uint16_t seq;        /* Sequence number */
} link_packet_t;

/* ------------------------------------------------------------------ */
/* State                                                               */
/* ------------------------------------------------------------------ */

static volatile link_state_t g_state = LINK_STATE_IDLE;

static uint8_t g_our_mac[6];
static uint8_t g_peer_mac[6];
static uint16_t g_seq = 0;

static vmupro_peernet_rx_ring_t *g_rx_ring = NULL;

/* Timing (microseconds) */
static uint64_t g_last_discovery_tx = 0;
static uint64_t g_last_ping_tx      = 0;
static uint64_t g_last_rx_time      = 0;

#define DISCOVERY_INTERVAL_US   100000   /* 100ms between broadcast packets */
#define PING_INTERVAL_US        500000   /* 500ms keepalive */
#define DISCONNECT_TIMEOUT_US  2000000   /* 2s no-packet timeout */

/* Serial transfer state (Milestone 3 — stubs for now) */
static volatile bool    g_transfer_pending = false;
static volatile bool    g_rx_ready         = false;
static volatile uint8_t g_rx_byte          = 0xFF;
static volatile uint8_t g_slave_sb         = 0xFF;

/* ------------------------------------------------------------------ */
/* Helpers                                                             */
/* ------------------------------------------------------------------ */

static void send_packet(const uint8_t *mac, uint8_t type, uint8_t payload) {
    link_packet_t pkt;
    pkt.magic[0] = LINK_MAGIC_0;
    pkt.magic[1] = LINK_MAGIC_1;
    pkt.type     = type;
    pkt.payload  = payload;
    pkt.seq      = g_seq++;
    vmupro_peernet_send(mac, (const uint8_t *)&pkt, sizeof(pkt));
}

static bool is_our_packet(const uint8_t *data, uint8_t len) {
    if (len != sizeof(link_packet_t)) return false;
    return (data[0] == LINK_MAGIC_0 && data[1] == LINK_MAGIC_1);
}

/* ------------------------------------------------------------------ */
/* Packet processing (called from link_cable_update on Core 1)        */
/* ------------------------------------------------------------------ */

static void process_packet(const uint8_t *mac, const link_packet_t *pkt) {
    switch (pkt->type) {
        case LINK_PKT_DISCOVER:
            if (g_state == LINK_STATE_DISCOVERING) {
                /* Peer found — send ACK and connect */
                memcpy(g_peer_mac, mac, 6);
                send_packet(g_peer_mac, LINK_PKT_DISCOVER_ACK, 0);
                g_last_rx_time = vmupro_get_time_us();
                g_state = LINK_STATE_CONNECTED;
            }
            break;

        case LINK_PKT_DISCOVER_ACK:
            if (g_state == LINK_STATE_DISCOVERING) {
                memcpy(g_peer_mac, mac, 6);
                g_last_rx_time = vmupro_get_time_us();
                g_state = LINK_STATE_CONNECTED;
            }
            break;

        case LINK_PKT_SERIAL_DATA:
            /* Milestone 3: remote master sent us a byte */
            if (g_state == LINK_STATE_CONNECTED) {
                /* Respond with our slave SB value */
                send_packet(g_peer_mac, LINK_PKT_SERIAL_ACK, g_slave_sb);

                /* Deliver master's byte to the emulator */
                g_rx_byte  = pkt->payload;
                g_rx_ready = true;
            }
            break;

        case LINK_PKT_SERIAL_ACK:
            /* Milestone 3: response from slave to our master transfer */
            if (g_state == LINK_STATE_CONNECTED && g_transfer_pending) {
                g_rx_byte          = pkt->payload;
                g_rx_ready         = true;
                g_transfer_pending = false;
            }
            break;

        case LINK_PKT_PING:
            /* Just a keepalive — rx_time updated below */
            break;

        case LINK_PKT_DISCONNECT:
            if (g_state == LINK_STATE_CONNECTED && memcmp(mac, g_peer_mac, 6) == 0) {
                g_state            = LINK_STATE_IDLE;
                g_transfer_pending = false;
                g_rx_ready         = false;
            }
            break;
    }

    /* Any valid packet from our peer resets the keepalive timer */
    if (g_state == LINK_STATE_CONNECTED && memcmp(mac, g_peer_mac, 6) == 0) {
        g_last_rx_time = vmupro_get_time_us();
    }
}

/* Drain all pending packets from the rx ring */
static void drain_rx_ring(void) {
    if (!g_rx_ring) return;

    while (g_rx_ring->read_idx != g_rx_ring->write_idx) {
        uint32_t rd = g_rx_ring->read_idx;
        vmupro_peernet_rx_slot_t *slot = &g_rx_ring->slots[rd];

        if (is_our_packet(slot->data, slot->len)) {
            link_packet_t pkt;
            memcpy(&pkt, slot->data, sizeof(pkt));
            process_packet(slot->mac, &pkt);
        }

        /* Read barrier before advancing index */
        __asm__ __volatile__("memw" ::: "memory");
        g_rx_ring->read_idx = (rd + 1) % VMUPRO_PEERNET_RX_RING_SIZE;
    }
}

/* ------------------------------------------------------------------ */
/* Public API — Lifecycle                                              */
/* ------------------------------------------------------------------ */

void link_cable_init(void) {
    if (g_state != LINK_STATE_IDLE) return;

    vmupro_peernet_init();
    vmupro_peernet_get_mac(g_our_mac);
    g_rx_ring = vmupro_peernet_get_rx_ring();

    g_transfer_pending = false;
    g_rx_ready         = false;
    g_rx_byte          = 0xFF;
    g_slave_sb         = 0xFF;
    g_seq              = 0;
}

void link_cable_start_discovery(void) {
    if (g_state != LINK_STATE_IDLE) return;

    g_last_discovery_tx = 0;
    g_state = LINK_STATE_DISCOVERING;
}

void link_cable_disconnect(void) {
    link_cable_deinit();
}

void link_cable_deinit(void) {
    if (g_state == LINK_STATE_CONNECTED) {
        send_packet(g_peer_mac, LINK_PKT_DISCONNECT, 0);
    }

    g_state            = LINK_STATE_IDLE;
    g_transfer_pending = false;
    g_rx_ready         = false;
    g_rx_ring          = NULL;

    vmupro_peernet_deinit();
}

/* ------------------------------------------------------------------ */
/* Public API — State                                                  */
/* ------------------------------------------------------------------ */

link_state_t link_cable_get_state(void) {
    return g_state;
}

const char* link_cable_get_status_text(void) {
    switch (g_state) {
        case LINK_STATE_DISCOVERING: return "Searching...";
        case LINK_STATE_CONNECTED:   return "Connected";
        default:                     return "Off";
    }
}

/* ------------------------------------------------------------------ */
/* Public API — Per-frame update                                       */
/* ------------------------------------------------------------------ */

void link_cable_update(void) {
    /* Process incoming packets */
    drain_rx_ring();

    uint64_t now = vmupro_get_time_us();

    if (g_state == LINK_STATE_DISCOVERING) {
        /* Broadcast discovery packets periodically */
        if (now - g_last_discovery_tx >= DISCOVERY_INTERVAL_US) {
            send_packet(NULL, LINK_PKT_DISCOVER, 0);
            g_last_discovery_tx = now;
        }
    }
    else if (g_state == LINK_STATE_CONNECTED) {
        /* Send keepalive pings */
        if (now - g_last_ping_tx >= PING_INTERVAL_US) {
            send_packet(g_peer_mac, LINK_PKT_PING, 0);
            g_last_ping_tx = now;
        }

        /* Check for disconnect timeout */
        if (now - g_last_rx_time >= DISCONNECT_TIMEOUT_US) {
            g_state            = LINK_STATE_IDLE;
            g_transfer_pending = false;
            g_rx_ready         = false;
        }
    }
}

/* ------------------------------------------------------------------ */
/* Public API — Serial transfer (Milestone 3 stubs)                    */
/* ------------------------------------------------------------------ */

void link_cable_master_transfer(uint8_t sb_out) {
    if (g_state != LINK_STATE_CONNECTED) return;
    g_transfer_pending = true;
    g_rx_ready         = false;
    send_packet(g_peer_mac, LINK_PKT_SERIAL_DATA, sb_out);
}

bool link_cable_master_poll(uint8_t *sb_in) {
    if (g_rx_ready && !g_transfer_pending) {
        *sb_in     = g_rx_byte;
        g_rx_ready = false;
        return true;
    }
    return false;
}

void link_cable_slave_set_sb(uint8_t sb_out) {
    g_slave_sb = sb_out;
}

bool link_cable_slave_poll(uint8_t *sb_in) {
    if (g_rx_ready) {
        *sb_in     = g_rx_byte;
        g_rx_ready = false;
        return true;
    }
    return false;
}
