#pragma once

#include <nx/kit/ini_config.h>

namespace nx {
namespace mobile_client {

struct Ini: public nx::kit::IniConfig
{
    Ini(): IniConfig("mobile_client.ini") {}

    NX_INI_FLAG(0, forceLiteMode, "Launch mobile_client in lite_mode on any platform.");
    NX_INI_FLAG(0, forceNonLiteMode, "Do not launch mobile_client in lite_mode on any platform.");
    NX_INI_FLAG(0, disableFullScreen, "In lite_mode, do not start in full-screen mode.");
    NX_INI_STRING("", logFile, "Path without .log. If empty, log goes to mobile_client.log next to this .ini, or Android logcat.");
    NX_INI_FLAG(0, enableLog, "Enable mobile_client log (DEBUG2 level by default) as defined by logFile.");
    NX_INI_FLAG(0, enableEc2TranLog, "Enable ec2_tran log (DEBUG2 level by default) as defined by logFile.");
    NX_INI_STRING("", logLevel, "Overrides (if defined) log level passed via command line.");
    NX_INI_FLAG(0, execAtGlThreadOnBeforeSynchronizing, "Connect lambda execution to the event.");
    NX_INI_FLAG(1, execAtGlThreadOnFrameSwapped, "Connect lambda execution to the event.");
    NX_INI_STRING("", tcpLogAddress, "Write log to specified IP:port as raw TCP stream");
};

inline Ini& ini()
{
    static Ini ini;
    return ini;
}

} // namespace mobile_client
} // namespace nx
