#pragma once

#include <nx/utils/flag_config.h>

namespace mobile_client {

struct FlagConfig: public nx::utils::FlagConfig
{
    using nx::utils::FlagConfig::FlagConfig;

    NX_FLAG(0, forceLiteMode, "Launch mobile_client in lite_mode, regardless of the platform.");
    NX_FLAG(0, disableFullScreen, "In lite_mode, do not start in full-screen mode.");
    NX_FLAG(0, enableLog, "Enable mobile_client logging to temp path (DEBUG2 level).");
    NX_FLAG(0, enableEc2TranLog, "Enable ec2_tran logging to temp path (DEBUG2 level).");
    NX_FLAG(0, execAtGlThreadOnBeforeSynchronizing, "Connect lambda execution to specified event.");
    NX_FLAG(1, execAtGlThreadOnFrameSwapped, "Connect lambda execution to specified event.");
};
extern FlagConfig conf;

} // namespace mobile_client

