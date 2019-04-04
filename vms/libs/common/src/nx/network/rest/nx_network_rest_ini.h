#pragma once

#include <nx/kit/ini_config.h>

namespace nx::network::rest {

struct Ini: nx::kit::IniConfig
{
    Ini(): IniConfig("nx_network_rest.ini") { reload(); }

    #if defined(_DEBUG)
        static constexpr int kAllowGetMethodReplacement = 1;
    #else
        static constexpr int kAllowGetMethodReplacement = 0;
    #endif

    NX_INI_FLAG(kAllowGetMethodReplacement, allowGetMethodReplacement,
        "Allows to change GET request method ot any other method by 'method_' URL parameter.\n"
        "So request 'GET /path?param=value&method_=post HTTP/1.1' is processed as POST.");

    // This must be disabled as soon as we can discontinue deprecated POST methods.
    NX_INI_FLAG(1, allowUrlParametersForAnyMethod,
        "Enables POST and PUT methods to accept URL parameters as well as the message body.");

    // This must be disabled as soon as we can discontinue deprecated modifying GET methods.
    NX_INI_FLAG(1, allowGetModifications,
        "Allows GET requests to modify server's state, e.g. 'GET /api/mergeSystems?... HTTP/1.1'\n"
        "will result in 'HTTP/1.1 403 Forbidden', so user is forced to use POST.");
};

inline Ini& ini()
{
    static Ini ini;
    return ini;
}

} // namespace nx::network::rest
