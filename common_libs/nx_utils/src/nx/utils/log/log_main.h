#pragma once

#include "log_logger.h"
#include "to_string.h"

namespace nx {
namespace utils {
namespace log {

/** @return Main logger. */
std::shared_ptr<Logger> NX_UTILS_API mainLogger();

/** Creates a logger for specific filters. */
std::shared_ptr<Logger> NX_UTILS_API addLogger(const std::set<QString>& filters);

/** @return Logger by tag or main if no specific logger is set. */
std::shared_ptr<Logger> NX_UTILS_API getLogger(const QString& tag, bool allowMain = true);

/**
 * Indicates if a message is going to be logged by any logger.
 */
template<typename Tag = QString>
bool isToBeLogged(Level level, const Tag& tag = {})
{
    const auto tagString = ::toString(tag);
    return getLogger(tagString)->isToBeLogged(level, tagString);
}

/**
 * Calculate and log message if it's supposed to be logged.
 */
#define NX_UTILS_LOG(LEVEL, TAG, MESSAGE) do \
{ \
    const auto tag = ::toString(TAG); \
    const auto logger = nx::utils::log::getLogger(tag); \
    if (logger->isToBeLogged(LEVEL, tag)) \
        logger->log(LEVEL, tag, ::toString(MESSAGE)); \
} while (0)

#define NX_ALWAYS(TAG, MESSAGE) NX_UTILS_LOG(nx::utils::log::Level::always, TAG, MESSAGE)
#define NX_ERROR(TAG, MESSAGE) NX_UTILS_LOG(nx::utils::log::Level::error, TAG, MESSAGE)
#define NX_WARNING(TAG, MESSAGE) NX_UTILS_LOG(nx::utils::log::Level::warning, TAG, MESSAGE)
#define NX_INFO(TAG, MESSAGE) NX_UTILS_LOG(nx::utils::log::Level::info, TAG, MESSAGE)
#define NX_DEBUG(TAG, MESSAGE) NX_UTILS_LOG(nx::utils::log::Level::debug, TAG, MESSAGE)
#define NX_VERBOSE(TAG, MESSAGE) NX_UTILS_LOG(nx::utils::log::Level::verbose, TAG, MESSAGE)

} // namespace log
} // namespace utils
} // namespace nx
