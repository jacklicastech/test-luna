/**
 * Starts any subsystems needed to initialize ethernet communications, shuts
 * them down when the service stops, and sends periodic status updates when
 * e.g. an IP address becomes known.
 *
 * This service does not receive any messages.
 *
 * Only one message is published by this service, but it can be published
 * multiple times depending on the number of ethernet interfaces in use by
 * this device. It takes the following form:
 *
 *     ["ethernet", "[name]", [ip]", "[netmask]", "[gateway]"]
 *
 * Frame 2 contains the name of this interface. Blank when the interface goes
 * down or the cable is removed.
 * 
 * Frame 3 contains the IP address of this interface. Blank when the interface
 * goes down or the cable is removed.
 *
 * Frame 4 contains the netmask of this interface. Blank when the interface
 * goes down or the cable is removed.
 *
 * Frame 5 contains the IP address of the default gateway for this interface,
 * if there is one. It is blank otherwise.
 *
 **/

#include "config.h"
#include <arpa/inet.h>
#include <czmq.h>
#include <errno.h>
#include <ifaddrs.h>
#include <memory.h>
#include <net/if.h>
#include <netinet/in.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include "plugin.h"

#define BROADCAST_INTERVAL 5000

#if HAVE_CTOS
  #include <ctosapi.h>
#endif

static zactor_t *service = NULL;

int check_link(char *ifname) {
  int socId = socket(AF_INET, SOCK_DGRAM, IPPROTO_IP);
  if (socId < 0) {
    LWARN("ethernet: check link: %s: socket failed: %s", ifname, strerror(errno));
    return 0;
  }

  struct ifreq if_req;
  (void) strncpy(if_req.ifr_name, ifname, sizeof(if_req.ifr_name));
  int rv = ioctl(socId, SIOCGIFFLAGS, &if_req);
  close(socId);

  if (rv == -1) {
    LWARN("ethernet: check link: %s: ioctl failed: %s", ifname, strerror(errno));
    return 0;
  }

  return (if_req.ifr_flags & IFF_UP) && (if_req.ifr_flags & IFF_RUNNING);
}

const char *find_gateway(const char *iface) {
  FILE *f;
  char line[100] , *p , *c, *g, *saveptr;
  const char *result = NULL;

  f = fopen("/proc/net/route" , "r");

  while (fgets(line, 100, f)) {
    p = strtok_r(line , " \t", &saveptr);
    c = strtok_r(NULL , " \t", &saveptr);
    g = strtok_r(NULL , " \t", &saveptr);

    if (p != NULL && c != NULL && !strcmp(p, iface)) {
      if(strcmp(c, "00000000") == 0) {
        if (g) {
          char *pEnd;
          int ng=strtol(g,&pEnd,16);
          struct in_addr addr;
          addr.s_addr=ng;
          result = inet_ntoa(addr);
        }
        break;
      }
    }
  }

  fclose(f);
  return result;
}

void ethernet_service(zsock_t *pipe, void *arg) {
  (void) arg;
  zsock_t *bcast = zsock_new_pub(">" EVENTS_PUB_ENDPOINT);
  zpoller_t *poller = zpoller_new(pipe, NULL);

#if HAVE_CTOS && HAVE_ETHERNET
  int res = CTOS_EthernetOpenEx();
  if (res != d_OK) {
    LWARN("ethernet: error %x while initializing device, service will not run", (unsigned) res);
    return;
  }

  BYTE b = '1';
  LTRACE("ethernet: dhcp: %x", (unsigned) CTOS_EthernetConfigSet(d_ETHERNET_CONFIG_DHCP, &b, 1));
#endif

#if HAVE_ETHERNET
  LINFO("ethernet: service initialized");
  zsock_signal(pipe, 0);

  while (1) {
    void *in = zpoller_wait(poller, BROADCAST_INTERVAL);
    if (!zpoller_expired(poller)) {
      LWARN("ethernet: service interrupted!");
      break;
    }

    if (in) {
      LDEBUG("ethernet: received shutdown signal");
      break;
    }

    char name[IF_NAMESIZE], ip[INET_ADDRSTRLEN], netmask[INET_ADDRSTRLEN],
         gateway[INET_ADDRSTRLEN],
         dhcp[32];
    memset(name,    0, sizeof(name));
    memset(ip,      0, sizeof(ip));
    memset(netmask, 0, sizeof(netmask));
    memset(gateway, 0, sizeof(gateway));
    memset(dhcp,    0, sizeof(dhcp));

    struct ifaddrs *ifa_start = NULL;
    int res = getifaddrs(&ifa_start);
    if (res != 0) {
      LWARN("ethernet: could not query available interfaces: %x", (unsigned) res);
      continue;
    }

    struct ifaddrs *ia = ifa_start;
    while (ia) {
      if (ia->ifa_addr->sa_family == AF_INET) {
        memset(name,    0, IF_NAMESIZE);
        memset(ip,      0, INET_ADDRSTRLEN);
        memset(netmask, 0, INET_ADDRSTRLEN);
        memset(gateway, 0, INET_ADDRSTRLEN);
        sprintf(name, "%s", ia->ifa_name);

        // If interface is down or cable is unplugged, we'll broadcast all
        // blank values even if we would otherwise have had (probably stale)
        // values.
        if (check_link(name)) {
          if (!inet_ntop(AF_INET, &(((struct sockaddr_in *) (ia->ifa_addr))->sin_addr), ip, INET_ADDRSTRLEN)) {
            LWARN("ethernet: could not parse IP address for interface %s: %s", name, strerror(errno));
            ia = ia->ifa_next;
            continue;
          }

          if (!inet_ntop(AF_INET, &(((struct sockaddr_in *) (ia->ifa_netmask))->sin_addr), netmask, INET_ADDRSTRLEN)) {
            LWARN("ethernet: could not parse netmask for interface %s: %s", name, strerror(errno));
            ia = ia->ifa_next;
            continue;
          }

          const char *gw = find_gateway(name);
          if (gw == NULL) {
            // probably not an error, we get this all the time for loopback
            // LWARN("ethernet: could not parse gateway for interface %s", name);
          } else {
            sprintf(gateway, "%s", gw);
          }
        }

        zsock_send(bcast, "sssss", "ethernet", name, ip, netmask, gateway, dhcp);
      }

      ia = ia->ifa_next;
    }
  }
#else
  LWARN("ethernet: not available on this device");
#endif

#if HAVE_CTOS && HAVE_ETHERNET
  res = CTOS_EthernetClose();
  if (res != d_OK) {
    LWARN("ethernet: error %x while shutting down device", (unsigned) res);
  }
#endif

  zsock_destroy(&bcast);
  zpoller_destroy(&poller);
  LINFO("ethernet: service shutdown complete");
}

int init_ethernet_service() {
  if (!service) service = zactor_new(ethernet_service, NULL);
  else { LWARN("ethernet: service already running"); }
  assert(service);
  return 0;
}

bool is_ethernet_service_running(void) { return !!service; }

void shutdown_ethernet_service() {
  if (service) zactor_destroy(&service);
  else { LWARN("ethernet: service is not running"); }
  service = NULL;
}
