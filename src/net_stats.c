/*
 * Implementation of low-level network helpers.
 * Keeps stdio/stdlib separated from main program flow.
 */

#include "bandwidth3.h"
#include <stdio.h>
#include <limits.h>    // for PATH_MAX
#include <string.h>
#include <sys/stat.h>
#include <stdlib.h>

/* Split comma-separated interface list into array. Returns count.       */
size_t parse_ifaces(const char *arg, char *ifaces[MAX_IFACES]) {
  if (!arg || !*arg)
    return 0u;

  char *dup = strdup(arg);
  if (!dup)
    return 0u;

  size_t n = 0u;
  char *saveptr = NULL, *tok = strtok_r(dup, ",", &saveptr);

  while (tok && n < MAX_IFACES) {
    size_t len = strnlen(tok, IFNAMSIZ - 1);
    if (len) {
      ifaces[n] = strndup(tok, len);
      if (ifaces[n])
        ++n;
    }
    tok = strtok_r(NULL, ",", &saveptr);
  }
  free(dup);
  return n;
}

/* --------------------------------------------------------------------- */
/* Read /proc/net/dev and aggregate counters for requested ifaces.       */
bool read_aggregate_stats(char *const ifaces[], size_t n_ifaces,
                          NetStats *out) {
  if (!out)
    return false;

  FILE *fp = fopen("/proc/net/dev", "r");
  if (!fp)
    return false;

  char line[256];
  /* Skip first two header lines */
  fgets(line, sizeof line, fp);
  fgets(line, sizeof line, fp);

  uint64_t rx_total = 0, tx_total = 0;

  while (fgets(line, sizeof line, fp)) {
    char ifname[IFNAMSIZ];
    uint64_t rx, tx;

    if (sscanf(line, " %[^:]: %lu %*u %*u %*u %*u %*u %*u %*u %lu", ifname, &rx,
               &tx) != 3)
      continue;

    /* If no filter list given, accept all except loopback */
    bool wanted = (n_ifaces == 0 && strcmp(ifname, "lo") != 0);

    for (size_t i = 0; !wanted && i < n_ifaces; ++i)
      wanted = (strcmp(ifname, ifaces[i]) == 0);

    if (wanted) {
      rx_total += rx;
      tx_total += tx;
    }
  }
  fclose(fp);

  out->rx_bytes = rx_total;
  out->tx_bytes = tx_total;
  return true;
}

/* --------------------------------------------------------------------- */
/* Average byte delta over refresh period -> bytes per second.           */
double avg_rate(uint64_t now, uint64_t old, unsigned sec_delta) {
  return (sec_delta == 0u) ? 0.0 : (double)(now - old) / sec_delta;
}

/* --------------------------------------------------------------------- */
/* Pretty-print a byte/s figure with SI or IEC prefixes.                  */
#include <inttypes.h>
#include <stdio.h>
void human_print(double bps, char unit, unsigned divisor, uint64_t warn,
                 uint64_t crit) {
  /* Convert to bits if requested */
  if (unit == 'b')
    bps *= 8.0;

  /* Threshold markers for Polybar colouring                        */
  if (crit && bps > crit)
    fputc('!', stdout); /* critical */
  else if (warn && bps > warn)
    fputc('?', stdout); /* warning  */

  const char *suffix = (unit == 'b') ? "b/s" : "B/s";
  const char *prefix[] = {"", "K", "M", "G", "T"};
  size_t i = 0u;

  while (bps >= divisor && i < 4u) {
    bps /= divisor;
    ++i;
  }
  printf("%7.1f %s%s", bps, prefix[i], suffix);
}
/* --------------------------------------------------------------------- */
bool read_iface_stats(const char *ifname, NetStats *out) {
  FILE *fp = fopen("/proc/net/dev", "r");
  if (!fp)
    return false;

  char line[256];
  /* skip header lines */
  fgets(line, sizeof line, fp);
  fgets(line, sizeof line, fp);

  bool ok = false;
  while (fgets(line, sizeof line, fp)) {
    char name[IFNAMSIZ];
    uint64_t rx, tx;

    if (sscanf(line, " %[^:]: %lu %*u %*u %*u %*u %*u %*u %*u %lu", name, &rx,
               &tx) != 3)
      continue;

    if (strcmp(name, ifname) == 0) {
      out->rx_bytes = rx;
      out->tx_bytes = tx;
      ok = true;
      break;
    }
  }
  fclose(fp);
  return ok;
}

/* --- avg_rate & human_print unchanged from previous commit ----------- */
