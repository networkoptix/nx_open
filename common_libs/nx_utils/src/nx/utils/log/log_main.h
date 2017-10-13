#pragma once

#include "log_logger.h"
#include "to_string.h"

namespace nx {
namespace utils {
namespace log {

/** @return Main logger. */
NX_UTILS_API std::shared_ptr<Logger> mainLogger();

/** Creates a logger for specific filters. */
NX_UTILS_API std::shared_ptr<Logger> addLogger(const std::set<QString>& filters);

/** @return Logger by tag or main if no specific logger is set. */
NX_UTILS_API std::shared_ptr<Logger> getLogger(const QString& tag, bool allowMainLogger = true);

/** Removes specific loggers for filters, so such messagess will go to main log. */
NX_UTILS_API void removeLoggers(const std::set<QString>& filters);

/** Get the maximum log level currently used by any logger registered via addLogger(). */
NX_UTILS_API Level maxLevel();

/**
 * Indicates if a message is going to be logged by any logger.
 */
template<typename Tag = QString>
bool isToBeLogged(Level level, const Tag& tag = {})
{
    using ::toString;
    const auto tagString = toString(tag);
    return getLogger(tagString)->isToBeLogged(level, tagString);
}

namespace detail {

class NX_UTILS_API Helper
{
public:
    Helper(Level level, const QString& tag);
    void log(const QString& message);
    explicit operator bool() const;

protected:
   const Level m_level;
   const QString m_tag;
   std::shared_ptr<Logger> m_logger;
};

template<typename Tag>
Helper makeHelper(Level level, const Tag& tag)
{
    using ::toString;
    return Helper(level, toString(tag));
}

class NX_UTILS_API Stream: public Helper
{
public:
    Stream(Level level, const QString& tag);
    ~Stream();

    /** Oprator logic is reversed to support tricky syntax: if (stream) {} else stream << */
    explicit operator bool() const;

    template<typename Value>
    Stream& operator<<(const Value& value)
    {
        using ::toString;
        m_strings << toString(value);
        return *this;
    }

private:
    QStringList m_strings;
};

template<typename Tag>
Stream makeStream(Level level, const Tag& tag)
{
    using ::toString;
    return Stream(level, toString(tag));
}

} // namespace detail

#define NX_UTILS_LOG_MESSAGE(LEVEL, TAG, MESSAGE) do \
{ \
    if (static_cast<nx::utils::log::Level>(LEVEL) <= nx::utils::log::maxLevel()) \
    { \
        if (auto helper = nx::utils::log::detail::makeHelper((LEVEL), (TAG))) \
            helper.log(::toString(MESSAGE)); \
    } \
} while (0)

#define NX_UTILS_LOG_STREAM(LEVEL, TAG) \
    if (auto stream = nx::utils::log::detail::makeStream((LEVEL), (TAG))) {} else stream <<

#define NX_UTILS_LOG(...) \
    NX_MSVC_EXPAND(NX_GET_4TH_ARG(__VA_ARGS__, \
        NX_UTILS_LOG_MESSAGE, NX_UTILS_LOG_STREAM, args_required)(__VA_ARGS__))

/**
 * Usage:
 *     NX_<LEVEL>(TAG) MESSAGE [<< ...]; //< Writes MESSAGE to log if LEVEL and TAG allow.
 *     NX_<LEVEL>(TAG, MESSAGE); //< The same as above, but shorter syntax;
 *
 * Examples:
 *     NX_ERROR(this) "Unexpected value" << value;
 *     NX_INFO(this, lm("Expected value %1").arg(value));
 */
#define NX_ALWAYS(...) NX_UTILS_LOG(nx::utils::log::Level::always, __VA_ARGS__)
#define NX_ERROR(...) NX_UTILS_LOG(nx::utils::log::Level::error, __VA_ARGS__)
#define NX_WARNING(...) NX_UTILS_LOG(nx::utils::log::Level::warning, __VA_ARGS__)
#define NX_INFO(...) NX_UTILS_LOG(nx::utils::log::Level::info, __VA_ARGS__)
#define NX_DEBUG(...) NX_UTILS_LOG(nx::utils::log::Level::debug, __VA_ARGS__)
#define NX_VERBOSE(...) NX_UTILS_LOG(nx::utils::log::Level::verbose, __VA_ARGS__)

} // namespace log
} // namespace utils
} // namespace nx
