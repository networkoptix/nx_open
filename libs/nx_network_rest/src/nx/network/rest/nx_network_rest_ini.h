// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <nx/kit/ini_config.h>

namespace nx::network::rest {

struct Ini: nx::kit::IniConfig
{
    Ini(): IniConfig("nx_network_rest.ini") { reload(); }

    // TODO: Change the default to false when we can discontinue deprecated POST methods.
    NX_INI_FLAG(true, allowUrlParametersForAnyMethod,
        "Enables POST and PUT methods to accept URL parameters as well as the message body.");

    static constexpr int kDefaultMaxSessionAgeForPrivilegedApiS = 60 * 60; //< 1 hour.
    NX_INI_INT(kDefaultMaxSessionAgeForPrivilegedApiS, maxSessionAgeForPrivilegedApiS,
        "Maximum session age for new APIs with Administrator permissions.\n"
        "Ignored if longer than the default.");

    std::chrono::seconds maxSessionAgeForPrivilegedApi() const
    {
        const std::chrono::seconds value(maxSessionAgeForPrivilegedApiS);
        const std::chrono::seconds default_(kDefaultMaxSessionAgeForPrivilegedApiS);
        return (value.count() <= 0 || value > default_) ? default_ : value;
    }

    NX_INI_FLAG(true, auditOnlyParameterNames,
        "Audit parameter values along with names if false.");
};

NX_NETWORK_REST_API Ini& ini();

} // namespace nx::network::rest
