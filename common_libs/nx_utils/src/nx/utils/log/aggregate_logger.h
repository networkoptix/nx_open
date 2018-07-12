#pragma once

#include <initializer_list>

#include "abstract_logger.h"

namespace nx {
namespace utils {
namespace log {

/**
 * Hides multiple loggers behind itself.
 */
class NX_UTILS_API AggregateLogger:
    public AbstractLogger
{
public:
    AggregateLogger(std::initializer_list<std::unique_ptr<AbstractLogger>> loggers);
};

} // namespace log
} // namespace utils
} // namespace nx
