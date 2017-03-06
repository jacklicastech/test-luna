#define _GNU_SOURCE
#include "config.h"
#include "plugins/autoupdate.h"

#ifdef DEBIAN

void autoupdate_reboot(void) {
  LINFO("autoupdate: NOT restarting application on debian");
}

int autoupdate_install(char * const *filenames, int num_filenames) {
  int i;
  for (i = 0; i < num_filenames; i++)
    LINFO("autoupdate: update file %s is ready, but won't be installed automatically on debian", filenames[i]);
  return 0;
}

#endif // DEBIAN
