// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <nx/kit/ini_config.h>

namespace nx::vms_server_plugins::analytics::mqtt_proxy {

struct Ini: public nx::kit::IniConfig
{
    Ini(): IniConfig("mqtt_proxy.ini") { reload(); }

    NX_INI_FLAG(true, enableOutput, "");
    NX_INI_INT(
        5, appReplyTimeotS, "Timeout in seconds for waiting for a reply from the Python app.");
};

Ini& ini();

} // nx::vms_server_plugins::analytics::mqtt_proxy
