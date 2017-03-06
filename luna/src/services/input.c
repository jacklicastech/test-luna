#include "config.h"
#include "services.h"
#ifdef HAVE_CTOS
#include <ctosapi.h>
#endif

// how many times per second to poll for user input. Higher means worse
// battery life. FIXME can we select() on a device instead?
#define FREQUENCY 10
#define BATTERY_POLL_DELAY 5000

static zactor_t *service = NULL;

#ifdef HAVE_CTOS
  static void input_service(zsock_t *pipe, void *arg) {
    zsock_t *msr_pub = zsock_new_pub(INPUT_MSR_ENDPOINT);
    zsock_t *battery_pub = zsock_new_pub(INPUT_BATTERY_ENDPOINT);
    zpoller_t *poller = zpoller_new(pipe, NULL);
    BYTE track1[128], track2[128], track3[128];
    token_id track1_token, track2_token, track3_token;
    USHORT track1len = 128, track2len = 128, track3len = 128;
    USHORT res;
    int is_emv = 0;
    char buf[128];
    char masked[128];
    long wait_ms = 1000 / FREQUENCY;
    int timer = 0, i;
    struct {
        BYTE percentage;
        DWORD charging;
    } battery = { 0, 0 };

    LINFO("input: initializing service");
    zsock_signal(pipe, 0);

    while (1) {
      void *in = zpoller_wait(poller, wait_ms);

      if (!in) {
          if (!zpoller_expired(poller)) {
              LWARN("input: service interrupted");
              break;
          } else {
              timer += wait_ms;
          }
      }

      if (in == pipe) {
        LDEBUG("input: received shutdown signal");
        break;
      }
      
      // check for battery data periodically
      if (timer > BATTERY_POLL_DELAY) {
        LDEBUG("input: checking battery stats");
        timer = 0;
        
        if ((res = CTOS_BatteryStatus(&(battery.charging))) != d_OK) {
          battery.charging = 0;
          LWARN("input: error %x while querying battery charge status", res);
        }

        if ((res = CTOS_BatteryGetCapacity(&(battery.percentage))) == d_OK) {
          sprintf(buf, "%u", battery.percentage);
          zsock_send(battery_pub, "sss", "capacity-changed", "percentage", buf);
        } else {
          // d_BATTERY_INVALID just means we can't query right now, usually
          // (always?) because charging is in progress.
          if (res != d_BATTERY_INVALID) {
            LWARN("input: error %x while querying current battery capacity", res);
          }
        }

        if (battery.charging & d_MK_BATTERY_CHARGE) {
          zsock_send(battery_pub, "s", "charging-started");
        } else {
          zsock_send(battery_pub, "s", "charging-stopped");
          if ((res = CTOS_BatteryGetCapacity(&(battery.percentage))) != d_OK) {
            LWARN("input: error %x while querying battery capacity", res);
          }
        }
      }

      // check for MSR data
      track1len = track2len = track3len = 128;
      res = CTOS_MSRRead(track1, &track1len, track2, &track2len, track3, &track3len);
      switch(res) {
        case d_OK:
          track1[track1len] = (BYTE) '\0';
          track2[track2len] = (BYTE) '\0';
          track3[track3len] = (BYTE) '\0';
          LTRACE("input: MSR swipe detected");
          LINSEC("input: MSR track 1: %s", (char *) track1);
          LINSEC("input: MSR track 2: %s", (char *) track2);
          LINSEC("input: MSR track 3: %s", (char *) track3);
          memset(masked, 0, sizeof(masked));
          is_emv = 0;

          // We have to check both track1 and track2 for service codes, as
          // they are needed to tell what is an EMV card and some EMV cards
          // seem to lack track 2. I don't know if they can lack track 1 so
          // I'll be cautious and assume they can.
          int delim_cnt = 0;
          // This was helpful:
          // http://blog.opensecurityresearch.com/2012/02/deconstructing-credit-cards-data.html
          for (i = 0; i < track1len; i++) {
            if (track1[i] == '^') delim_cnt++;
            if (delim_cnt == 2) {
              // after finding the second delimeter, we should have enough
              // space for YYMMSSS, where YYMM is the expiry year/month and
              // SSS is the service code.
              if (track1len - i >= 5) {
                if (track2[i + 5] == '2' || track2[i + 5] == '6') {
                  LTRACE("input: an EMV card was swiped according to track 1");
                  is_emv = 1;
                }
              }
              break;
            }
          }

          // offsets here are to skip start sentinel / end sentinel / CRC.
          if (track2len > 3) {
            // parse and mask the PAN. First find the equals sign. Everything
            // before it is the PAN.
            for (i = 1; i < track2len - 3; i++)
              if (track2[i] == '=') break;
            if (i < track2len - 3) {
              // we found it, just make sure track has enough characters for
              // mask. If offsets check out, we've also found the expiry date
              // and (importantly) service code. Service code is used to check
              // if this is an EMV card or not.
              if ((track2len - 3) - i >= 7) {
                if (track2[i + 5] == '2' || track2[i + 5] == '6') {
                  LTRACE("input: an EMV card was swiped according to track 2");
                  is_emv = 1;
                }
              }

              if (track2len - 3 > 5) {
                masked[0] = *(track2 + 1);
                sprintf(((char *) masked) + 1, "***********");
                masked[12] = *(track2 + track2len - 2 - 4);
                masked[13] = *(track2 + track2len - 2 - 3);
                masked[14] = *(track2 + track2len - 2 - 2);
                masked[15] = *(track2 + track2len - 2 - 1);
              } else {
                sprintf(masked, "4***********1111");
              }
            } else {
              sprintf(masked, "4***********1111");
            }
          }

          if (track1len > 3) track1_token = create_token(track1 + 1, track1len - 3, masked);
          else track1_token = create_token("", 0, "<track:1>");
          if (track2len > 3) track2_token = create_token(track2 + 1, track2len - 3, masked);
          else track2_token = create_token("", 0, "<track:2>");
          if (track3len > 3) track3_token = create_token(track3 + 1, track3len - 3, masked);
          else track3_token = create_token("", 0, "<track:3>");
          sprintf((char *) track1, "%u", track1_token);
          sprintf((char *) track2, "%u", track2_token);
          sprintf((char *) track3, "%u", track3_token);
          zsock_send(msr_pub, "ssssssssi", "swipe", "track1", (char *) track1,
                                                    "track2", (char *) track2,
                                                    "track3", (char *) track3,
                                                    "is_emv", is_emv);
          break;
        case d_MSR_TRACK_ERROR:
          LDEBUG("input: MSR track read error detected");
          zsock_send(msr_pub, "s", "swipe-track-error");
          break;
        case d_MSR_NO_SWIPE:
          // no swipe occurred
          break;
        default:
          LWARN("input: unexpected MSR read result: %x", res);
      }
    }

    LINFO("input: terminating service");
    zsock_destroy(&battery_pub);
    zsock_destroy(&msr_pub);
    zpoller_destroy(&poller);

    LINFO("input: shutdown complete");
  }
#else // HAVE_CTOS
  static void input_service(zsock_t *pipe, void *arg) {
    LWARN("input: not supported on this device");
  }
#endif // HAVE_CTOS

int init_input_service() {
  service = zactor_new(input_service, NULL);
  assert(service);
  return 0;
}

void shutdown_input_service() {
  zactor_destroy(&service);
}
