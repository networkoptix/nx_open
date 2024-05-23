// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <string>
#include <vector>

#include <nx/reflect/instrument.h>
#include <nx/utils/log/log_level.h>

/**
 * Types in this namespace provide log management through the HTTP REST API.
 */
namespace nx::network::maintenance::log {

/**%apidoc Filter that a message has to match to be printed. */
struct Filter
{
    /**%apidoc Max log level of messages matched by this filter.
     * Possible values: "error", "warning", "info", "debug", "verbose", "trace".
     */
    std::string level;

    /**%apidoc A list of accepted message prefixes. */
    std::vector<std::string> tags;
};

#define Filter_Fields (level)(tags)

NX_REFLECTION_INSTRUMENT(Filter, Filter_Fields)

//-------------------------------------------------------------------------------------------------

/**%apidoc Specifies output for matched log messages. */
struct Logger
{
    /**%apidoc Id of this logger to. It is used to reference the logger in the API. */
    int id = -1;

    /**%apidoc Path to log file on a local filesystem to write messages to. */
    std::string path;

    /**%apidoc List of filters specifying messages that will be passed to this logger. */
    std::vector<Filter> filters;

    /**%apidoc When a message is not matched by any filter, but it has a log level less or equal
     * to this value, then the message is still printed.
     * To disable this behavior, set this value to "none".
     */
    std::string defaultLevel;
};

#define Logger_Fields (id)(path)(filters)(defaultLevel)

NX_REFLECTION_INSTRUMENT(Logger, Logger_Fields)

//-------------------------------------------------------------------------------------------------

using LoggerList = std::vector<Logger>;

} // namespace nx::network::maintenance::log
