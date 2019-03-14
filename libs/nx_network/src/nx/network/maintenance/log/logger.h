#pragma once

#include <string>
#include <vector>

#include <nx/fusion/model_functions_fwd.h>
#include <nx/utils/log/log_level.h>

namespace nx::network::maintenance::log {

// TODO: #Nate add documentation please. What's that? Third way to setup the same logs?
struct Filter
{
    std::string level;

    // TODO: #Nate Actually regexps can be written here. Suggesting to rename to 'filters'.
    std::vector<std::string> tags;
};

#define Filter_Fields (level)(tags)

QN_FUSION_DECLARE_FUNCTIONS(Filter, (json), NX_NETWORK_API)

//-------------------------------------------------------------------------------------------------

struct Logger
{
    int id = -1;
    std::string path;
    std::vector<Filter> filters;
    std::string defaultLevel;
};

#define Logger_Fields (id)(path)(filters)(defaultLevel)

QN_FUSION_DECLARE_FUNCTIONS(Logger, (json), NX_NETWORK_API)

//-------------------------------------------------------------------------------------------------

/**
 * This type is needed to be able to produce JSON document like
 * { "loggers": [....] }.
 */
struct Loggers
{
    std::vector<Logger> loggers;
};

#define Loggers_Fields (loggers)

QN_FUSION_DECLARE_FUNCTIONS(Loggers, (json), NX_NETWORK_API)

} // namespace nx::network::maintenance::log
