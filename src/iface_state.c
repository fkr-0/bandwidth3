/*
 * Low-level helpers for link-state detection.
 */
#include "bandwidth3.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>

/* --------------------------------------------------------------------- */
/* Wi-Fi test: directory /sys/class/net/<iface>/wireless exists          */
bool is_wireless(const char *ifname) {
  char path[256];
  snprintf(path, sizeof path, "/sys/class/net/%s/wireless", ifname);

  struct stat st;
  return (stat(path, &st) == 0) && S_ISDIR(st.st_mode);
}

/* --------------------------------------------------------------------- */
/* Helpers for Wi-Fi association:                                        */
static bool wifi_has_link(const char *ifname) {
  FILE *fp = fopen("/proc/net/wireless", "r");
  if (!fp)
    return false;

  char line[256];
  /* Skip headers (first two lines) */
  fgets(line, sizeof line, fp);
  fgets(line, sizeof line, fp);

  bool linked = false;
  while (fgets(line, sizeof line, fp)) {
    if (strncmp(line, ifname, strlen(ifname)) == 0) {
      unsigned link;
      /* field 3 is link quality */
      if (sscanf(line + strlen(ifname), ":%*d %u", &link) == 1)
        linked = link > 0;
      break;
    }
  }
  fclose(fp);
  return linked;
}

/* --------------------------------------------------------------------- */
/* Get current SSID for a wireless interface                             */
void get_wifi_ssid(const char *ifname, char *ssid_buf) {
  ssid_buf[0] = '\0'; // Default to empty string
  int sockfd = socket(AF_INET, SOCK_DGRAM, 0);
  if (sockfd < 0) {
    perror("socket");
    return;
  }

  struct iwreq wreq;
  memset(&wreq, 0, sizeof(struct iwreq));
  strncpy(wreq.ifr_name, ifname, IFNAMSIZ -1);

  if (ioctl(sockfd, SIOCGIWESSID, &wreq) == 0) {
    strncpy(ssid_buf, wreq.u.essid.pointer, IW_ESSID_MAX_SIZE);
    ssid_buf[IW_ESSID_MAX_SIZE] = '\0'; // Ensure null termination
  } else {
    // perror("ioctl SIOCGIWESSID"); // Optional: print error if ioctl fails
  }
  close(sockfd);
}

/* --------------------------------------------------------------------- */
/* Return current interface state                                        */
IfState get_iface_state(const char *ifname, bool wifi_hint) {
  char path[256], buf[32];

  /* operstate: "up", "down", "dormant", ... */
  snprintf(path, sizeof path, "/sys/class/net/%s/operstate", ifname);
  FILE *fp = fopen(path, "r");
  if (!fp)
    return IFSTATE_ERR;

  if (!fgets(buf, sizeof buf, fp)) {
    fclose(fp);
    return IFSTATE_ERR;
  }
  fclose(fp);

  if (strncmp(buf, "down", 4) == 0)
    return IFSTATE_DISABLED;

  /* At this point interface is administratively UP */
  snprintf(path, sizeof path, "/sys/class/net/%s/carrier", ifname);
  fp = fopen(path, "r");
  if (fp) {
    int carrier = fgetc(fp);
    fclose(fp);
    return (carrier == '1') ? IFSTATE_CONNECTED : IFSTATE_DISCONNECTED;
  }

  /* Some Wi-Fi drivers omit 'carrier' – fall back to /proc/net/wireless */
  if (wifi_hint)
    return wifi_has_link(ifname) ? IFSTATE_CONNECTED : IFSTATE_DISCONNECTED;

  return IFSTATE_ERR;
}
