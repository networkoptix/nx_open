// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <nx/kit/ini_config.h>

namespace nx::vms::rules {

struct NX_VMS_RULES_API Ini: public nx::kit::IniConfig
{
    Ini(): IniConfig("nx_vms_rules.ini") { reload(); }

    // Both engines are required for Cloud notifications to work in 5.1.
    NX_INI_STRING("old", rulesEngine, "Version of VMS Rules Engine (old / new / both)");

    // TODO: #amalov Temporary solution for 5.1. Should be removed in the future.
    NX_INI_FLAG(false, fullSupport,
        "Process cloud notification only when set to false.");

    NX_INI_FLAG(false, showSystemRules,
        "Show system rules in the rules editor.");
};

NX_VMS_RULES_API Ini& ini();

} // namespace nx::vms::rules
