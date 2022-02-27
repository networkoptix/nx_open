// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <nx/kit/ini_config.h>

namespace nx::p2p {

struct Ini: public nx::kit::IniConfig
{
    Ini(): IniConfig("nx_p2p.ini") { reload(); }

    NX_INI_FLAG(false, forceWebSocketForClient,
        "Force to use webSocket for clients in spite of user settings to enable/disable webSocket "
        "on the advanced tab.");
};

inline Ini& ini()
{
    static Ini ini;
    return ini;
}

} // namespace nx::p2p
