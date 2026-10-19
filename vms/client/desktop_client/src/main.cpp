// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include <nx/build_info.h>
#include <nx/vms/client/desktop/application.h>
#include <nx/vms/client/desktop/ini.h>

#if defined(Q_OS_MACOS)
    #include "mac_bundle_launcher.h"
#endif

int main(int argc, char** argv)
{
#if defined(Q_OS_MACOS)
    if (nx::vms::client::desktop::forwardMacBundleLaunch(&argc, argv))
        return 0;
#endif

    return nx::vms::client::desktop::runApplication(argc, argv);
}
