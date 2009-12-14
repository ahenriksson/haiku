/*
 * Copyright 2009, Colin Günther, coling@gmx.de.
 * All Rights Reserved. Distributed under the terms of the MIT License.
 */


#include <sys/bus.h>
#include <sys/kernel.h>


HAIKU_FBSD_WLAN_DRIVER_GLUE(marvell8335, malo, pci)

NO_HAIKU_CHECK_DISABLE_INTERRUPTS();
NO_HAIKU_REENABLE_INTERRUPTS();
NO_HAIKU_FBSD_MII_DRIVER();

HAIKU_DRIVER_REQUIREMENTS(FBSD_TASKQUEUES | FBSD_FAST_TASKQUEUE | FBSD_WLAN);
HAIKU_FIRMWARE_VERSION(0);
