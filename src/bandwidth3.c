/*
 * bandwidth3  v0.1.3 – network‑throughput module for Polybar
 *
 *  ┌────────────────────────────────────────────────────────────────────┐
 *  │   Priority for configuration                                      │
 *  │      1. Built‑in defaults                                         │
 *  │      2. Environment variables                                     │
 *  │      3. Command‑line arguments                                    │
 *  └────────────────────────────────────────────────────────────────────┘
 *
 *  Build:   make            # debug/optimised binary in ./build/
 *  Install: make install    # PREFIX=/usr/local
 *  Tests:   make test       # very light self‑checks (shell)
 */

#ifndef BANDWIDTH3_VERSION
#define BANDWIDTH3_VERSION "0.1.3"
#endif

#include "bandwidth3.h"
#include <errno.h>
#include <getopt.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>
#include <inttypes.h>    // for SCNu64, PRIu64, etc.

/* forward‐declare parse_ifaces (defined later in this file) */
static size_t parse_ifaces(const char *ifaces_raw, char **vec);
// parse comma-separated interface list into vec
static size_t parse_ifaces(const char *ifaces_raw, char **vec) {
    size_t count = 0;
    char *tmp = strdup(ifaces_raw);
    if (!tmp) return 0;
    char *token = strtok(tmp, ",");
    while (token && count < 32) {
        vec[count++] = token;
        token = strtok(NULL, ",");
    }
    return count;
}

enum { STATE_OK, STATE_WARNING, STATE_CRITICAL, STATE_UNKNOWN };

static volatile sig_atomic_t g_run = 1;
static void sigint_handler(int sig) {
  (void)sig;
  g_run = 0;
}

/* --------------------------------------------------------------------- */
static void print_usage(const char *argv0) {
  printf("bandwidth3 %s\n", BANDWIDTH3_VERSION);
  printf("Usage: %s [options]\n\n", argv0);
  printf("  -b | -B        Bits/s or Bytes/s (default Bytes)\n");
  printf("  -t <sec>       Refresh time (default 1)\n");
  printf("  -i <list>      Interfaces to monitor (comma‑separated)\n");
  printf("  -W <rx:tx>     Warning thresholds (Bytes/s)\n");
  printf("  -C <rx:tx>     Critical thresholds (Bytes/s)\n");
  printf("  -s             Use SI divisor (1000) instead of IEC (1024)\n");
  printf("  --wifi-only    Restrict to wireless adapters\n");
  printf("  --eth-only     Restrict to wired adapters\n");
  printf("  -V, --version  Show version and exit\n");
  printf("  -h, --help     This help text\n\n");
  printf("Environment overrides: USE_BITS, USE_BYTES, USE_SI, REFRESH_TIME,\n"
         "  INTERFACES, WARN_RX, WARN_TX, CRIT_RX, CRIT_TX, WIFI_ONLY, "
         "ETH_ONLY\n");
}

/* ---------------------------------------------------------------------
 * Helper: safely parse an unsigned 64‑bit integer from environment
 * ------------------------------------------------------------------- */
static void env_to_u64(const char *name, uint64_t *dst) {
  const char *v = getenv(name);
  if (v && *v) {
    char *end = NULL;
    uint64_t tmp = strtoull(v, &end, 10);
    if (end && *end == '\0')
      *dst = tmp;
  }
}

/* ---------------------------------------------------------------------
 * Apply environment variables.  Defaults should be set beforehand; CLI
 * will run afterwards (CLI overrides).
 * ------------------------------------------------------------------- */
static void apply_env(char *unit, unsigned *refresh, unsigned *divisor,
                      uint64_t *warn_rx, uint64_t *warn_tx, uint64_t *crit_rx,
                      uint64_t *crit_tx, char **ifaces_raw, bool *wifi_only,
                      bool *eth_only) {
  const char *v;

  /* ---- Units ---------------------------------------------------- */
  v = getenv("USE_BITS");
  if (v && *v == '1')
    *unit = 'b';
  v = getenv("USE_BYTES");
  if (v && *v == '1')
    *unit = 'B';

  /* ---- Refresh time -------------------------------------------- */
  v = getenv("REFRESH_TIME");
  if (v && *v)
    *refresh = (unsigned)strtoul(v, NULL, 10);

  /* ---- Interface filter ---------------------------------------- */
  v = getenv("INTERFACES");
  if (!v)
    v = getenv("INTERFACE");
  if (v && *v)
    *ifaces_raw = (char *)v; /* env string is static */

  /* ---- Thresholds --------------------------------------------- */
  env_to_u64("WARN_RX", warn_rx);
  env_to_u64("WARN_TX", warn_tx);
  env_to_u64("CRIT_RX", crit_rx);
  env_to_u64("CRIT_TX", crit_tx);

  /* ---- Divisor ------------------------------------------------- */
  v = getenv("USE_SI");
  if (v && *v == '1')
    *divisor = 1000u;

  /* ---- Transport filter --------------------------------------- */
  v = getenv("WIFI_ONLY");
  if (v && *v == '1')
    *wifi_only = true;
  v = getenv("ETH_ONLY");
  if (v && *v == '1')
    *eth_only = true;
}

/* --------------------------------------------------------------------- */
static const char *state_icon(bool wifi, IfState st) {
  const char *base = wifi ? "" : ""; /* Nerd Font glyphs */
  switch (st) {
  case IFSTATE_CONNECTED:
    return base;
  case IFSTATE_DISCONNECTED:
    return "⚠";
  case IFSTATE_DISABLED:
    return "✗";
  default:
    return "�";
  }
}

// --- Format strategy for Ethernet adapters: only print when connected
static bool print_eth(Iface *n, double rx, double tx,
                      char unit, unsigned divisor,
                      uint64_t warn_rx, uint64_t warn_tx,
                      uint64_t crit_rx, uint64_t crit_tx) {
    if (n->state != IFSTATE_CONNECTED)
        return false;

    printf("[ %s | ", state_icon(n->wifi, n->state));
    human_print(rx, unit, divisor, warn_rx, crit_rx);
    printf(" ");
    human_print(tx, unit, divisor, warn_tx, crit_tx);
    printf(" ]");
    return true;
}

// --- Format strategy for Wi-Fi adapters: hide when disabled, show disconnected when idle
static bool print_wifi(Iface *n, double rx, double tx,
                       char unit, unsigned divisor,
                       uint64_t warn_rx, uint64_t warn_tx,
                       uint64_t crit_rx, uint64_t crit_tx) {
    if (n->state == IFSTATE_DISABLED)
        return false;

    printf("[ %s", state_icon(n->wifi, n->state));
    if (n->state == IFSTATE_CONNECTED) {
        if (n->ssid[0] != '\0') {
            printf(" %s", n->ssid);
        }
        printf(" | ");
        human_print(rx, unit, divisor, warn_rx, crit_rx);
        printf(" ");
        human_print(tx, unit, divisor, warn_tx, crit_tx);
    } else if (n->state == IFSTATE_DISCONNECTED) {
        printf(" | [no-link]");
    } else { // IFSTATE_ERR or other
        printf(" | [err]");
    }
    printf(" ]");
    return true;
}

/* --------------------------------------------------------------------- */
int main(int argc, char *argv[]) {
  /* ---------- 1. Defaults --------------------------------------- */
  char unit = 'B';
  unsigned refresh = 1u;
  unsigned divisor = 1024u;
  uint64_t warn_rx = 0, warn_tx = 0;
  uint64_t crit_rx = 0, crit_tx = 0;
  bool wifi_only = false, eth_only = false;
  char *ifaces_raw = NULL; /* may point to env string */

  /* ---------- 2. Environment (override defaults) ---------------- */
  apply_env(&unit, &refresh, &divisor, &warn_rx, &warn_tx, &crit_rx, &crit_tx,
            &ifaces_raw, &wifi_only, &eth_only);

  /* ---------- 3. CLI parsing (override env) --------------------- */
  static const struct option long_opts[] = {{"wifi-only", no_argument, 0, 1},
                                            {"eth-only", no_argument, 0, 2},
                                            {"help", no_argument, 0, 'h'},
                                            {"version", no_argument, 0, 'V'},
                                            {0, 0, 0, 0}};

  int opt, idx;
  while ((opt = getopt_long(argc, argv, "bBsht:i:W:C:V", long_opts, &idx)) !=
         -1) {
    switch (opt) {
    case 'b':
      unit = 'b';
      break;
    case 'B':
      unit = 'B';
      break;
    case 't':
      refresh = (unsigned)strtoul(optarg, NULL, 10);
      break;
    case 'i':
      ifaces_raw = optarg;
      break;
    case 'W':
      sscanf(optarg, "%" SCNu64 ":%" SCNu64, &warn_rx, &warn_tx);
      break;
    case 'C':
      sscanf(optarg, "%" SCNu64 ":%" SCNu64, &crit_rx, &crit_tx);
      break;
    case 's':
      divisor = 1000u;
      break;
    case 1:
      wifi_only = true;
      break;
    case 2:
      eth_only = true;
      break;
    case 'V':
      printf("%s\n", BANDWIDTH3_VERSION);
      return 0;
    case 'h':
      print_usage(argv[0]);
      return 0;
    default:
      print_usage(argv[0]);
      return STATE_UNKNOWN;
    }
  }

  /* Disallow conflicting filters */
  if (wifi_only && eth_only) {
    fputs("Cannot combine --wifi-only and --eth-only\n", stderr);
    return STATE_UNKNOWN;
  }

  /* ---------- Interface list ------------------------------------ */
  char *vec[32] = {0};
  size_t n_ifaces = 0;

  if (ifaces_raw)
    n_ifaces = parse_ifaces(ifaces_raw, vec);

  if (n_ifaces == 0) {
    fputs("No interfaces specified (use -i or INTERFACES env)\n", stderr);
    return STATE_UNKNOWN;
  }

  /* ---------- Build Iface array --------------------------------- */
  Iface ifs[32];
  for (size_t i = 0; i < n_ifaces; ++i) {
    strncpy(ifs[i].name, vec[i], IFNAMSIZ);
    ifs[i].wifi = is_wireless(vec[i]);
    ifs[i].state = IFSTATE_ERR;
    ifs[i].prev = (NetStats){0};
    ifs[i].ssid[0] = '\0'; // Initialize SSID to empty
    read_iface_stats(vec[i], &ifs[i].prev);
  }

  signal(SIGINT, sigint_handler);
  signal(SIGTERM, sigint_handler);

  /* ---------- Main loop ----------------------------------------- */
  while (g_run) {
    sleep(refresh);
    bool first_printed_interface = true;
    for (size_t i = 0; i < n_ifaces; ++i) {
      Iface *n = &ifs[i];
      n->state = get_iface_state(n->name, n->wifi);
      if (n->wifi && n->state == IFSTATE_CONNECTED) {
        get_wifi_ssid(n->name, n->ssid);
      } else {
        n->ssid[0] = '\0'; // Clear SSID if not connected or not wifi
      }

      NetStats cur;
      if (!read_iface_stats(n->name, &cur))
        n->state = IFSTATE_ERR;

      double rx = avg_rate(cur.rx_bytes, n->prev.rx_bytes, refresh);
      double tx = avg_rate(cur.tx_bytes, n->prev.tx_bytes, refresh);
      n->prev = cur;

      bool printed_this_interface = n->wifi
          ? print_wifi(n, rx, tx, unit, divisor, warn_rx, warn_tx, crit_rx, crit_tx)
          : print_eth(n, rx, tx, unit, divisor, warn_rx, warn_tx, crit_rx, crit_tx);

      if (printed_this_interface) {
        if (!first_printed_interface) {
          printf(" | "); // Separator between interface groups
        }
        first_printed_interface = false;
      }
    }
    if (!first_printed_interface) { // Only print newline if something was printed
        putchar('\n');
    }
    fflush(stdout);
  }

  return STATE_OK;
}
