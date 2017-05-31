#pragma once

/**@file
 * New logging system to replace log.h.
 *
 * Usage:
 * <pre><code>
 *     NX_<LEVEL>(TAG) << MESSAGE; //< Writes MESSAGE to log if LEVEL and TAG allow.
 *     NX_<LEVEL>(TAG, MESSAGE); //< The same as above, but shorter syntax;
 * </code></pre>
 */

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

/** Removes specific loggers for filters, so such messagess will go to main log. */
void NX_UTILS_API removeLoggers(const std::set<QString>& filters);

/**
 * Indicates if a message is going to be logged by any logger.
 */
template<typename Tag = QString>
bool isToBeLogged(Level level, const Tag& tag = {})
{
    const auto tagString = ::toString(tag);
    return getLogger(tagString)->isToBeLogged(level, tagString);
}

#define NX_UTILS_LOG_MESSAGE(LEVEL, TAG, MESSAGE) do \
{ \
    const auto tag = ::toString(TAG); \
    const auto logger = nx::utils::log::getLogger(tag); \
    if (logger->isToBeLogged(LEVEL, tag)) \
        logger->log(LEVEL, tag, ::toString(MESSAGE)); \
} while (0)

template<typename Tag>
class Stream
{
public:
    Stream(Level level, const Tag& tag): m_level(level), m_tag(tag) {}
    ~Stream(){ NX_UTILS_LOG_MESSAGE(m_level, m_tag, m_strings.join(' ')); }

    template<typename Value>
    Stream& operator<<(const Value& value)
    {
        using ::toString;
        m_strings << toString(value);
        return *this;
    }

private:
    const Level m_level;
    const Tag& m_tag;
    QStringList m_strings;
};

#define NX_UTILS_LOG_STREAM(LEVEL, TAG) \
    if (!isToBeLogged(LEVEL, TAG)) {} else nx::utils::log::Stream<decltype(TAG)>(LEVEL, TAG)

#define NX_UTILS_LOG(...) \
    NX_MSVC_EXPAND(NX_GET_4TH_ARG(__VA_ARGS__, \
        NX_UTILS_LOG_MESSAGE, NX_UTILS_LOG_STREAM, args_required)(__VA_ARGS__))

#define NX_ALWAYS(...) NX_UTILS_LOG(nx::utils::log::Level::always, __VA_ARGS__)
#define NX_ERROR(...) NX_UTILS_LOG(nx::utils::log::Level::error, __VA_ARGS__)
#define NX_WARNING(...) NX_UTILS_LOG(nx::utils::log::Level::warning, __VA_ARGS__)
#define NX_INFO(...) NX_UTILS_LOG(nx::utils::log::Level::info, __VA_ARGS__)
#define NX_DEBUG(...) NX_UTILS_LOG(nx::utils::log::Level::debug, __VA_ARGS__)
#define NX_VERBOSE(...) NX_UTILS_LOG(nx::utils::log::Level::verbose, __VA_ARGS__)

} // namespace log
} // namespace utils
} // namespace nx
