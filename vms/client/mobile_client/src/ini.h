// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <nx/kit/ini_config.h>

namespace nx {
namespace mobile_client {

struct Ini: public nx::kit::IniConfig
{
    Ini(): IniConfig("mobile_client.ini") {}

    NX_INI_STRING("", logFile, "Path without .log. If empty, log goes to mobile_client.log next to this .ini, or Android logcat.");
    NX_INI_FLAG(0, enableLog, "Enable mobile_client log (DEBUG2 level by default) as defined by logFile.");
    NX_INI_STRING("", logLevel, "Overrides (if defined) log level passed via command line.");
    NX_INI_FLAG(0, execAtGlThreadOnBeforeSynchronizing, "Connect lambda execution to the event.");
    NX_INI_FLAG(1, execAtGlThreadOnFrameSwapped, "Connect lambda execution to the event.");
    NX_INI_STRING("", tcpLogAddress, "Write log to specified IP:port as raw TCP stream");
    NX_INI_INT(0, clientWebServerPort,
        "[CI] Enables web server to remotely control the Nx Client operation; port should be in\n"
        "range 1..65535 (typically 7012) to enable; 0 means disabled.");
};

Ini& ini();

} // namespace mobile_client
} // namespace nx
