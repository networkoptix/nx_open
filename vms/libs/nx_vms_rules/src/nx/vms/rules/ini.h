// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <nx/kit/ini_config.h>

namespace nx::vms::rules {

struct NX_VMS_RULES_API Ini: public nx::kit::IniConfig
{
    Ini(): IniConfig("nx_vms_rules.ini") { reload(); }

    NX_INI_FLAG(false, showSystemRules,
        "Show system rules in the rules editor.");

    NX_INI_FLAG(false, showHiddenTextFields,
        "Show hidden text fields in VMS rules dialog");

    NX_INI_INT(5, analyticsEventInactivityS,
        "Timeout in seconds for analytics events to auto generate Stop event.");
};

NX_VMS_RULES_API Ini& ini();

} // namespace nx::vms::rules
