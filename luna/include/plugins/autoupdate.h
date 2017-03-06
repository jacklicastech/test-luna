/* 
 * File:   autoupdate.h
 * Author: Colin
 *
 * Created on January 2, 2016, 9:15 PM
 */

#ifndef AUTOUPDATE_H
#define	AUTOUPDATE_H

#include <czmq.h>
#include "util/md5_helpers.h"
#include "plugin.h"

#define UPDATE_FAILED   1
#define REBOOT_REQUIRED 2

/*
 * Device-specific installation routine. Must return 0 for success,
 * or `UPDATE_FAILED`, or `REBOOT_REQUIRED`. May return
 * `UPDATE_FAILED | REBOOT_REQUIRED` if necessary.
 *
 * The function must guarantee that the update is transactional; that is,
 * all files must be installed or no changes will be made.
 */
int autoupdate_install(char * const *filenames, int num_filenames);

/*
 * Called when a previous call to `autoupdate_install` returned
 * `REBOOT_REQUIRED`, and now is a good time to do so.
 */
void autoupdate_reboot(void);

#endif	/* AUTOUPDATE_H */

