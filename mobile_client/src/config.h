#pragma once

#include <nx/utils/flag_config.h>

namespace mobile_client {

struct FlagConfig: public nx::utils::FlagConfig
{
    using nx::utils::FlagConfig::FlagConfig;

    NX_FLAG(0, enableEc2TranLog, "");

    NX_FLAG(0, execAtGlThreadOnBeforeSynchronizing, "Connect lambda execution to specified event.");
    NX_FLAG(1, execAtGlThreadOnFrameSwapped, "Connect lambda execution to specified event.");
};
extern FlagConfig conf;

} // namespace mobile_client

