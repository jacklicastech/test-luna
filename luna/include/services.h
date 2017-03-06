/* 
 * File:   services.h
 * Author: Colin
 *
 * Created on December 18, 2015, 2:30 AM
 */

#ifndef SERVICES_H
#define	SERVICES_H

#include "cli.h"
#include "io/signals.h"
#include "services/logger.h"
#include "services/settings.h"
#include "services/wifi.h"
#include "services/webserver.h"
#include "services/input.h"
#include "services/usb.h"
#include "services/bluetooth.h"
#include "services/timer.h"
#include "services/tokenizer.h"
#include "services/emv.h"
#include "services/events_proxy.h"
#include "services/touchscreen.h"

int init_plugins(const arguments_t *arguments, int argc, char *argv[]);
void shutdown_plugins(void);

#endif	/* SERVICES_H */

