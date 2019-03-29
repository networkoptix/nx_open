#pragma once

#include <nx/kit/ini_config.h>

namespace nx::network::rest {

// This ini config resides here because nx::network::rest is still in common and we can not
// instantiate single ini configs because of static linkage.
// Threre are already some plans to move nx::network::rest here anyway.

struct NX_UTILS_API Ini: nx::kit::IniConfig
{
    Ini(): IniConfig("nx_network_rest.ini") { reload(); }

    #if defined(_DEBUG)
        static constexpr int kAllowGetMethodReplacement = 1;
    #else
        static constexpr int kAllowGetMethodReplacement = 0;
    #endif

    NX_INI_FLAG(kAllowGetMethodReplacement, allowGetMethodReplacement,
        "Allows to change GET request method ot any other method by 'method_' URL paramiter.\n"
        "So request 'GET /path?param=value&method_=post HTTP/1.1'\n"
        "works like 'POST /path HTTP/1.1' with body 'param=value'.");

    // This must be disabled as soon as we can discontinue deprecated POST methods.
    NX_INI_FLAG(1, allowUrlParamitersForAnyMethod,
        "Enables POST and PUT methods to accept URL paramiters as well as message body.");
};

NX_UTILS_API Ini& ini();

} // namespace nx::network::rest
