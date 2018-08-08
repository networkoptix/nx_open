#pragma once

#include <nx/kit/ini_config.h>

struct TestCameraIni: public nx::kit::IniConfig
{
    TestCameraIni(): IniConfig("test_camera.ini") { reload(); }

    NX_INI_INT(4984, discoveryPort, "Port on which test camera expects discovery packets.");
    NX_INI_INT(4985, mediaPort, "Port on which test camera serves media stream.");
    NX_INI_STRING(
        "Network optix camera emulator 3.0 discovery",
        findMessage,
        "Message expected from server when discovering.");
    NX_INI_STRING(
        "Network optix camera emulator 3.0 responce",
        idMessage,
        "Message sent by camera when discovered.");
};

inline TestCameraIni& testCameraIni()
{
    static TestCameraIni ini;
    return ini;
}
