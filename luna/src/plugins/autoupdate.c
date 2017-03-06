/**
 * Given a set of files and MD5 checksums, verifies those files and then
 * installs them.
 *
 * Understands only 1 message, an events broadcast with the following format:
 *
 *     ["autoupdate", "install", "[filename1]", "[checksum1]", ...]
 *
 * Where frame 2 contains a string referencing a filename, and frame 2
 * contains a hex MD5 digest of the file's contents. If the checksums for any
 * of the specified files do not match, none of the files will be installed.
 *
 * When the update completes, the following message will be broadcast to the
 * events system:
 *
 *     ["autoupdate", "complete"]
 *
 * (Note: if a reboot or application restart is required, this message might
 * not be sent.)
 *
 * If the update fails, the following message will be broadcast instead:
 *
 *     ["autoupdate", "complete"]
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
#include "plugins/autoupdate.h"
#include "util/files.h"

static zactor_t *service = NULL;

void autoupdate_service(zsock_t *pipe, void *arg) {
  (void) arg;
  zsock_t *msg_recv = zsock_new_sub(">" EVENTS_SUB_ENDPOINT, "autoupdate");
  zsock_t *bcast = zsock_new_pub(">" EVENTS_PUB_ENDPOINT);
  zpoller_t *poller = zpoller_new(pipe, msg_recv, NULL);

  LINFO("autoupdate: service initialized");
  zsock_signal(pipe, 0);

  while (1) {
    void *in = zpoller_wait(poller, -1);
    if (!in) {
      LWARN("autoupdate: service interrupted!");
      break;
    }

    if (in == pipe) {
      LDEBUG("autoupdate: received shutdown signal");
      break;
    }

    char **files = NULL, **etags = NULL, **relative_filenames = NULL;
    int num_files = 0;
    int failed = 0;
    zmsg_t *msg = zmsg_recv(in);
    char *str = zmsg_popstr(msg);
    if (strcmp(str, "autoupdate")) {
      LWARN("autoupdate: BUG: received a message not prefixed with 'autoupdate', ignoring it");
      goto au_done;
    }
    free(str);

    str = zmsg_popstr(msg);
    // can happen when we receive our own broadcast, need to filter that out
    if (strcmp(str, "install")) {
      goto au_done;
    }
    free(str);
    str = NULL;

    LDEBUG("autoupdate: trying to update %d files", zmsg_size(msg) / 2);
    while (zmsg_size(msg) > 0) {
      char *relative_filename = zmsg_popstr(msg);
      char *etag = zmsg_popstr(msg);
      if (etag == NULL) {
        LERROR("autoupdate: no etag received for file %s", relative_filename);
        free(relative_filename);
        failed = 1;
        goto au_done;
      }

      char *full_filename = find_writable_file("files", relative_filename);
      if (!full_filename) {
        LERROR("autoupdate: could not expand filename %s", relative_filename);
        free(relative_filename);
        free(etag);
        failed = 1;
        goto au_done;
      }

      if (!md5_matches(full_filename, etag)) {
        LERROR("autoupdate: file %s checksum does not match %s", full_filename, etag);
        free(relative_filename);
        free(full_filename);
        free(etag);
        failed = 1;
        goto au_done;
      }

      files = realloc(files, (num_files + 1) * sizeof(char *));
      etags = realloc(etags, (num_files + 1) * sizeof(char *));
      relative_filenames = realloc(relative_filenames, (num_files + 1) * sizeof(char *));
      files[num_files] = full_filename;
      etags[num_files] = etag;
      relative_filenames[num_files] = relative_filename;
      num_files++;
    }

    // All files verified, just need to install them.

    // Because on some hardware we actually get a reboot before autoupdate_install
    // ever returns, we need to set values as if we already had a success, then
    // clear them if we get a failure. There are all kinds of reasons I don't
    // like this but my not liking it doesn't fix the problem.
    LDEBUG("autoupdate: about to install");
    zsock_t *settings = zsock_new_req(SETTINGS_ENDPOINT);
    int i;
    for (i = 0; i < num_files; i++) {
      char settings_key[PATH_MAX + 64];
      sprintf(settings_key, "autoupdate.current_release.%s", relative_filenames[i]);
      settings_set(settings, 1, settings_key, etags[i]);
    }

    int update_result = autoupdate_install(relative_filenames, num_files);

    if (update_result & UPDATE_FAILED) {
      LWARN("autoupdate: update failed");
      zsock_send(bcast, "ss", "autoupdate", "failed");
      // grrrrr
      for (i = 0; i < num_files; i++) {
        char settings_key[PATH_MAX + 64];
        sprintf(settings_key, "autoupdate.current_release.%s", relative_filenames[i]);
        settings_del(settings, 1, settings_key);
      }
    } else {
      LINFO("autoupdate: update complete");
      zsock_send(bcast, "ss", "autoupdate", "complete");
    }
    zsock_destroy(&settings);

    if (update_result & REBOOT_REQUIRED) {
      LINFO("auto-update: reboot required");
      autoupdate_reboot();
    }

au_done:
    if (failed)
      zsock_send(bcast, "ss", "autoupdate", "failed");
    if (files) {
      int i;
      for (i = 0; i < num_files; i++) {
        free(files[i]);
        free(etags[i]);
        free(relative_filenames[i]);
      }
      free(files);
      free(etags);
      free(relative_filenames);
    }

    if (str) free(str);
    if (msg) zmsg_destroy(&msg);
  }

  zsock_destroy(&bcast);
  zsock_destroy(&msg_recv);
  zpoller_destroy(&poller);
  LINFO("autoupdate: service shutdown complete");
}

int init_autoupdate_service() {
  if (!service) service = zactor_new(autoupdate_service, NULL);
  else { LWARN("autoupdate: service already running"); }
  assert(service);
  return 0;
}

bool is_autoupdate_service_running(void) { return !!service; }

void shutdown_autoupdate_service() {
  if (service) zactor_destroy(&service);
  else { LWARN("autoupdate: service is not running"); }
  service = NULL;
}
