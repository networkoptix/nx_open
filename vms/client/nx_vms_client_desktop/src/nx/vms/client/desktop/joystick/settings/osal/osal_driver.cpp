// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "osal_driver.h"

#include <nx/utils/log/log.h>
#include <nx/utils/thread/mutex.h>

#include "implementations/os_hid_driver_mac.h"
#include "implementations/os_winapi_driver_win.h"

namespace nx::vms::client::desktop::joystick {

OsalDriver* OsalDriver::getDriver()
{
    static nx::Mutex mutex;
    NX_MUTEX_LOCKER lock(&mutex);

    static OsalDriver* instance = nullptr;

    if (instance)
        return instance;

#if defined(Q_OS_WIN)
    instance = new OsWinApiDriver();
#elif defined(Q_OS_MAC)
    instance = new OsHidDriver();
#else
    NX_ASSERT(false, "OsalDriver::getDriver() should only be used in Windows and macOS.");
#endif

    return instance;
}

}
