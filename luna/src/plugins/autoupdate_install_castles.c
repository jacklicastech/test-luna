#define _GNU_SOURCE
#include "config.h"

#ifdef HAVE_CTOS

#include <ctosapi.h>

#include "plugins/autoupdate.h"
#include "util/files.h"

void autoupdate_reboot(void) {
  LINFO("autoupdate: rebooting device");
  CTOS_PowerMode(d_PWR_REBOOT);
}

int autoupdate_install(char * const *filenames, int num_filenames) {
  #define ERR(x...) { LERROR("autoupdate: " x); goto cleanup; }
  int err = UPDATE_FAILED;
  char *mci_filename = NULL, *mmci_filename = NULL;
  FILE *update_file = NULL;
  int i;

  LDEBUG("autoupdate: installing %d updates", num_filenames);

  mci_filename = find_writable_file("files", "update.mci");
  if (!mci_filename)  ERR("couldn't expand writable path for manifest file");

  mmci_filename = find_writable_file("files", "update.mmci");
  if (!mmci_filename) ERR("couldn't expand writable path for multiple manifest file");

  update_file = fopen(mmci_filename, "w");
  if (!update_file) ERR("couldn't open manifest file %s for writing", mmci_filename);
  fprintf(update_file, "update.mci\n");
  fclose(update_file);
  
  update_file = fopen(mci_filename, "w");
  if (!update_file) ERR("couldn't open multiple manifest file %s for writing", mci_filename);
  for (i = 0; i < num_filenames; i++)
    fprintf(update_file, "%s\n", filenames[i]);
  fclose(update_file);
  update_file = NULL;

  bool success = false;
  bool poll = true;
  unsigned short status;
  LINFO("autoupdate: installing file %s", mmci_filename);
  CTOS_UpdateFromMMCI((unsigned char *) mmci_filename, 1);
  err = 0;
  do {
    sleep(1);
    status = CTOS_UpdateGetResult();
    switch(status) {
      case d_CAP_UPDATE_FINISHED:
        LDEBUG("autoupdate: update finished");
        poll = false;
        success = true;
        err = err | 0; // success
        break;
      case d_CAP_RESET_REQUIRED:
        LDEBUG("autoupdate: update finished, reset required");
        poll = false;
        success = true;
        err = err | REBOOT_REQUIRED; // success with reboot
        break;
      case d_CAP_REBOOT_REQUIRED:
        LDEBUG("autoupdate: update finished, reboot required");
        poll = false;
        success = true;
        err = err | REBOOT_REQUIRED; // success with reboot
        break;
      case d_CAP_CONTINUE_DOWNLOAD:
        LDEBUG("autoupdate: still updating");
        break;
      default:
        LERROR("autoupdate: status %d (error)", status);
        err = err | UPDATE_FAILED;
        poll = false;
    }
  } while (poll);

cleanup:
  if (update_file)   fclose(update_file);
  if (mci_filename)  free(mci_filename);
  if (mmci_filename) free(mmci_filename);
  return err;
}

#endif // HAVE_CTOS
