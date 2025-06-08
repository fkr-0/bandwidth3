#pragma once
/* Public interface for bandwith3 – v1.1 */

#define _POSIX_C_SOURCE 200809L
#define _DEFAULT_SOURCE

#include <linux/if.h>       /* IFNAMSIZ */
#include <linux/wireless.h> // Added for IW_ESSID_MAX_SIZE
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define MAX_IFACES 32 // maximum number of interfaces for parsing

/* ---------- Generic per-interface counters --------------------------- */
typedef struct {
    uint64_t rx_bytes;
    uint64_t tx_bytes;
} NetStats;

/* ---------- Link status enumeration --------------------------------- */
typedef enum {
    IFSTATE_DISABLED,     /* operstate == down                         */
    IFSTATE_DISCONNECTED, /* up but no carrier / not associated        */
    IFSTATE_CONNECTED,    /* carrier present OR Wi-Fi associated       */
    IFSTATE_ERR           /* could not determine state                 */
} IfState;

/* ---------- Per-adapter object -------------------------------------- */
typedef struct {
    char name[IFNAMSIZ];
    bool wifi; /* determined once with is_wireless()      */
    IfState state;
    NetStats prev; /* previous snapshot for rate calculation  */
    char ssid[IW_ESSID_MAX_SIZE + 1]; // Added for storing SSID
} Iface;

/* ---------- Function prototypes (from net_stats.c) ----------------- */
bool read_iface_stats(const char *ifname, NetStats *out);

/* ---------- Function prototypes (from iface_state.c) --------------- */
bool is_wireless(const char *ifname);
IfState get_iface_state(const char *ifname, bool wifi_hint);
void get_wifi_ssid(const char *ifname, char *ssid_buf); // Added prototype

/* ---------- Function prototypes (from human_print.c) ------------- */
double avg_rate(uint64_t now, uint64_t old, unsigned sec_delta);
void human_print(double bytes_per_s, char unit, unsigned divisor, uint64_t warn,
                 uint64_t crit);
