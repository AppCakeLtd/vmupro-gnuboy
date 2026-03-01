/*
 * Wireless Link Cable — peer discovery and Game Boy serial data exchange
 * over VMUPro PeerNet (ESP-NOW transport).
 */

#pragma once

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    LINK_STATE_IDLE,        /* Not initialized or disconnected */
    LINK_STATE_DISCOVERING, /* Broadcasting, looking for a peer */
    LINK_STATE_CONNECTED,   /* Paired with a peer */
} link_state_t;

/* ---- Lifecycle ---- */
void link_cable_init(void);
void link_cable_start_discovery(void);
void link_cable_disconnect(void);
void link_cable_deinit(void);

/* ---- State ---- */
link_state_t link_cable_get_state(void);
const char*  link_cable_get_status_text(void);

/* ---- Per-frame update (keepalive + discovery tick) ---- */
void link_cable_update(void);

/* ---- Serial transfer (Milestone 3) ---- */
void link_cable_master_transfer(uint8_t sb_out);
bool link_cable_master_poll(uint8_t *sb_in);
void link_cable_slave_set_sb(uint8_t sb_out);
bool link_cable_slave_poll(uint8_t *sb_in);

#ifdef __cplusplus
}
#endif
