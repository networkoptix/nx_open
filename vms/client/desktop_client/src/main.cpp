// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include <nx/build_info.h>
#include <nx/vms/client/desktop/application.h>
#include <nx/vms/client/desktop/ini.h>

#if defined(_WIN32)
    // Make video drivers on laptops prefer discrete GPU if available.
    extern "C" {
    // Nvidia GPU hint.
    // http://developer.download.nvidia.com/devzone/devcenter/gamegraphics/files/OptimusRenderingPolicies.pdf
    __declspec(dllexport) unsigned long NvOptimusEnablement = 0x00000001;

    // AMD GPU hint.
    // http://developer.amd.com/community/blog/2015/10/02/amd-enduro-system-for-developers/
    __declspec(dllexport) int AmdPowerXpressRequestHighPerformance = 1;
    }
#endif

int main(int argc, char** argv)
{
    if (nx::build_info::isMacOsX()
        && QString(nx::vms::client::desktop::ini().graphicsApi) == "opengl")
    {
        // We do not rely on Mac OS OpenGL implementation-related throttling.
        // Otherwise all animations go faster.
        qputenv("QSG_RENDER_LOOP", "basic");
    }

    return nx::vms::client::desktop::runApplication(argc, argv);
}
