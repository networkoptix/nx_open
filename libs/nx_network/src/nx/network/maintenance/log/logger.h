#pragma once

#include <string>
#include <vector>

#include <nx/fusion/model_functions_fwd.h>
#include <nx/utils/log/log_level.h>

namespace nx::network::maintenance::log {

struct Filter
{
    std::string level;
    std::vector<std::string> tags;

    bool operator==(const Filter& rhs) const
    {
        return level == rhs.level && tags == rhs.tags;
    }
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

    bool operator==(const Logger& rhs) const
    {
        return id == rhs.id
            && path == rhs.path
            && filters == rhs.filters
            && defaultLevel == rhs.defaultLevel;
    }
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

    bool operator==(const Loggers& rhs)
    {
        return loggers == rhs.loggers;
    }
};

#define Loggers_Fields (loggers)

QN_FUSION_DECLARE_FUNCTIONS(Loggers, (json), NX_NETWORK_API)

} // namespace nx::network::maintenance::log
