#ifndef UTIL_MACHINE_ID_H
#define UTIL_MACHINE_ID_H

#ifdef __cplusplus
  extern "C" {
#endif

/*
 * Returns an ID representing this machine. The ID is unique to this machine
 * and will survive across reboots, but may not survive across OS reinstalls.
 *
 * Consumer must free the result.
 */
char *unique_machine_id();

#ifdef __cplusplus
  }
#endif

#endif // UTIL_MACHINE_ID_H
