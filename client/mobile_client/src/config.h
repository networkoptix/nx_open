#pragma once

#include <nx/utils/flag_config.h>

namespace mobile_client {

struct FlagConfig: public nx::utils::FlagConfig
{
    using nx::utils::FlagConfig::FlagConfig;

    NX_FLAG(0, forceLiteMode, "Launch mobile_client in lite_mode, regardless of the platform.");
    NX_FLAG(0, forceNonLiteMode, "Do not launch mobile_client in lite_mode, regardless of anything.");
    NX_FLAG(0, disableFullScreen, "In lite_mode, do not start in full-screen mode.");
    NX_STRING_PARAM("", logFile, "Path without .log. If empty: mobile_client in temp path / Android logcat.");
    NX_FLAG(0, enableLog, "Enable mobile_client logging (DEBUG2 level) as defined by logFile.");
    NX_FLAG(0, enableEc2TranLog, "Enable ec2_tran logging (DEBUG2 level) as defined by logFile.");
    NX_FLAG(0, execAtGlThreadOnBeforeSynchronizing, "Connect lambda execution to specified event.");
    NX_FLAG(1, execAtGlThreadOnFrameSwapped, "Connect lambda execution to specified event.");
};
extern FlagConfig conf;

} // namespace mobile_client

