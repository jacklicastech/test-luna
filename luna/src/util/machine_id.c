#include "config.h"
#include <stdlib.h>

#if HAVE_CTOS // Castles
  #include <ctosapi.h>
#elif HAVE_DBUS
  #include <dbus/dbus.h>
#endif

char *unique_machine_id() {
  char *out;

  #ifdef HAVE_CTOS // Castles
    out = calloc(16, sizeof(char));
    CTOS_GetFactorySN((unsigned char *) out);
    out[15] = '\0'; // don't send the check code
  #else // Generic (?) Linux
    out = dbus_get_local_machine_id();
  #endif

  return out;
}
