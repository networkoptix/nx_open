// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "os_hid_driver.h"

#include <nx/utils/log/log.h>
#include <nx/utils/thread/mutex.h>
#include <nx/vms/client/desktop/ini.h>

#include "implementations/os_hid_driver_mac.h"

namespace nx::vms::client::desktop::joystick {

OsHidDriver* OsHidDriver::getDriver()
{
    static nx::Mutex mutex;
    NX_MUTEX_LOCKER lock(&mutex);

    static OsHidDriver* instance = nullptr;

    if (instance)
        return instance;

#if defined(Q_OS_MAC)
    instance = new OsHidDriverMac();
#else
    NX_ASSERT(false, "OsHidDriver::getDriver() should only be used in macOS.");
#endif

    return instance;
}

}
