// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <nx/kit/ini_config.h>

struct Ini: public nx::kit::IniConfig
{
    Ini(): IniConfig("nx_vms_rules.ini") { reload(); }

    NX_INI_STRING("old", rulesEngine, "Version of VMS Rules Engine (old / new / both)");
};

Ini& ini();
