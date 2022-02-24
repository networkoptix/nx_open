// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include <nx/vms/client/desktop/application.h>

#include <nx/build_info.h>

int main(int argc, char** argv)
{
    if (nx::build_info::isMacOsX())
    {
        // We do not rely on Mac OS OpenGL implementation-related throttling.
        // Otherwise all animations go faster.
        qputenv("QSG_RENDER_LOOP", "basic");
    }

    return nx::vms::client::desktop::runApplication(argc, argv);
}
