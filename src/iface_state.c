/*
 * Low-level helpers for link-state detection.
 */
#include "bandwidth3.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/ioctl.h> // Added for ioctl
#include <unistd.h>    // Added for close
#include <sys/socket.h> // Added for socket function and AF_INET, SOCK_DGRAM
#include <netinet/in.h> // Added for struct sockaddr_in, not strictly needed for SIOCGIWESSID but good practice for socket programming
#include <linux/if.h> // Added for IFNAMSIZ and struct ifreq (though iwreq is used from wireless.h)

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
  strncpy(wreq.ifr_name, ifname, IFNAMSIZ - 1);

  char essid_buffer[IW_ESSID_MAX_SIZE + 1]; // Buffer to store the ESSID
  wreq.u.essid.pointer = essid_buffer;
  wreq.u.essid.length = IW_ESSID_MAX_SIZE + 1;
  // wreq.u.essid.flags = 0; // Not strictly necessary for SIOCGIWESSID but good practice

  if (ioctl(sockfd, SIOCGIWESSID, &wreq) == 0) {
    // Check if an ESSID was actually returned (length > 0)
    // The kernel sets wreq.u.essid.length to the actual length of the SSID
    if (wreq.u.essid.length > 0) {
        // Ensure null termination based on actual length returned by kernel
        // or max buffer size, whichever is smaller.
        size_t actual_len = (wreq.u.essid.length < IW_ESSID_MAX_SIZE) ? wreq.u.essid.length : IW_ESSID_MAX_SIZE;
        memcpy(ssid_buf, essid_buffer, actual_len); // Changed from strncpy
        ssid_buf[actual_len] = '\0';
    } else {
        ssid_buf[0] = '\0'; // No SSID or empty SSID
    }
  } else {
    // perror("ioctl SIOCGIWESSID"); // Keep this commented for now unless further debugging is needed
    ssid_buf[0] = '\0'; // Ensure ssid_buf is empty on error
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
