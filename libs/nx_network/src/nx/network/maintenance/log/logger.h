// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <string>
#include <vector>

#include <nx/reflect/instrument.h>
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

NX_REFLECTION_INSTRUMENT(Filter, Filter_Fields)

//-------------------------------------------------------------------------------------------------

struct Logger
{
    int id = -1;
    std::string path;
    std::vector<Filter> filters;
    std::string defaultLevel;
};

#define Logger_Fields (id)(path)(filters)(defaultLevel)

NX_REFLECTION_INSTRUMENT(Logger, Logger_Fields)

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

NX_REFLECTION_INSTRUMENT(Loggers, Loggers_Fields)

} // namespace nx::network::maintenance::log
