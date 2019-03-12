#pragma once

#include "log_logger.h"

#include <nx/utils/system_error.h>

namespace nx {
namespace utils {
namespace log {

/** @return Main logger. */
NX_UTILS_API std::shared_ptr<AbstractLogger> mainLogger();

NX_UTILS_API bool setMainLogger(std::unique_ptr<AbstractLogger> logger);

NX_UTILS_API bool addLogger(
    std::unique_ptr<AbstractLogger> logger,
    bool writeLogHeader = true);

/** Every setMainLogger and addLogger calls will fail until unlockConfiguration is called. */
NX_UTILS_API void lockConfiguration();

/** Allow setMainLogger and addLogger calls after lockConfiguration. */
NX_UTILS_API void unlockConfiguration();

/** @return AbstractLogger by tag or main if no specific logger is set. */
NX_UTILS_API std::shared_ptr<AbstractLogger> getLogger(const Tag& tag);

/** @return AbstractLogger by exact tag, nullptr otherwise. */
NX_UTILS_API std::shared_ptr<AbstractLogger> getExactLogger(const Tag& tag);

/** Removes specific loggers for filters, so such messagess will go to main log. */
NX_UTILS_API void removeLoggers(const std::set<Filter>& filters);

/** Get the maximum log level currently used by any logger registered via addLogger(). */
NX_UTILS_API Level maxLevel();

/** Indicates if a message is going to be logged by any logger. */
bool NX_UTILS_API isToBeLogged(Level level, const Tag& tag = {});

/** Indicates if passwords should be shown in logs. */
bool NX_UTILS_API showPasswords();

namespace detail {

class Helper
{
public:
    Helper() {} //< Constructing a helper which does not log anything.

    Helper(Level level, Tag tag):
        m_level(level),
        m_tag(std::move(tag)),
        m_logger(getLogger(m_tag))
    {
        if (!m_logger->isToBeLogged(m_level, m_tag))
            m_logger.reset();
    }

    void log(const QString& message) { m_logger->logForced(m_level, m_tag, message); }

    explicit operator bool() const { return m_logger.get(); }

protected:
   const Level m_level = Level::none;
   const Tag m_tag;
   std::shared_ptr<AbstractLogger> m_logger;
};

class Stream: public Helper
{
public:
    using Helper::Helper;

    Stream() = default;
    Stream(const Stream&) = delete;
    Stream(Stream&&) = default;
    Stream& operator=(const Stream&) = delete;
    Stream& operator=(Stream&&) = delete;

    Stream& setDelimiter(const QString& delimiter)
    {
        m_delimiter = delimiter;
        return *this;
    }

    template<typename Value>
    Stream& operator<<(const Value& value)
    {
        using ::toString;
        m_strings << toString(value);
        return *this;
    }

    void flush()
    {
        if (!m_logger)
            return;

        NX_ASSERT(!m_strings.isEmpty());
        log(m_strings.join(m_delimiter));
        m_logger.reset();
    }

private:
    QStringList m_strings;
    QString m_delimiter = QStringLiteral(" ");
};

inline Stream makeStream(Level level, const Tag& tag)
{
    if (level > maxLevel())
        return Stream();
    return Stream(level, tag);
}

} // namespace detail

/**
 * NOTE: Preserves current system error code (SystemError::getLastOSErrorCode()).
 */
#define NX_UTILS_LOG_MESSAGE(LEVEL, TAG, ...) do \
{ \
    struct ScopeTag{}; /*< Used by NX_SCOPE_TAG to get scope from demangled type_info::name(). */ \
    if (static_cast<nx::utils::log::Level>(LEVEL) <= nx::utils::log::maxLevel()) \
    { \
        const auto systemErrorBak = SystemError::getLastOSErrorCode(); \
        if (auto helper = nx::utils::log::detail::Helper((LEVEL), (TAG))) \
            helper.log(nx::utils::log::makeMessage(__VA_ARGS__)); \
        SystemError::setLastErrorCode(systemErrorBak); \
    } \
} while (0)

#define NX_UTILS_LOG_STREAM(LEVEL, TAG) \
    for (auto stream = nx::utils::log::detail::makeStream((LEVEL), (TAG)); stream; stream.flush()) \
        stream /* <<... */

/**
 * Can be used to redirect nx_kit's NX_PRINT to log as following:
 * <pre><code>
 *     #define NX_PRINT NX_UTILS_LOG_STREAM_NO_SPACE( \
 *         nx::utils::log::Level::debug, \
 *         nx::utils::log::Tag(QStringLiteral("my_plugin")) \
 *     ) << NX_PRINT_PREFIX
 * </code></pre>
 */
#define NX_UTILS_LOG_STREAM_NO_SPACE(LEVEL, TAG) \
    for (auto stream = nx::utils::log::detail::makeStream((LEVEL), (TAG)); stream; stream.flush()) \
        stream.setDelimiter(QString()) /* <<... */

#define NX_UTILS_LOG(...) \
    NX_MSVC_EXPAND(NX_GET_15TH_ARG(__VA_ARGS__, \
        NX_UTILS_LOG_MESSAGE, NX_UTILS_LOG_MESSAGE, NX_UTILS_LOG_MESSAGE, NX_UTILS_LOG_MESSAGE, \
        NX_UTILS_LOG_MESSAGE, NX_UTILS_LOG_MESSAGE, NX_UTILS_LOG_MESSAGE, NX_UTILS_LOG_MESSAGE, \
        NX_UTILS_LOG_MESSAGE, NX_UTILS_LOG_MESSAGE, NX_UTILS_LOG_MESSAGE, NX_UTILS_LOG_MESSAGE, \
        NX_UTILS_LOG_STREAM, args_required)(__VA_ARGS__))

/*
 * Usage:
 *     NX_<LEVEL>(TAG, MESSAGE [, VALUES...]; //< Writes MESSAGE to log if LEVEL and TAG allow.
 *
 * Examples:
 *     NX_INFO(this, "Expected value %1", value);
 *     NX_DEBUG(NX_SCOPE_TAG, "Message from a non-member function.");
 *
 * Deprecated usage:
 *     NX_ERROR(this) << "Unexpected value" << value;
 */
#define NX_ALWAYS(...) NX_UTILS_LOG(nx::utils::log::Level::always, __VA_ARGS__)
#define NX_ERROR(...) NX_UTILS_LOG(nx::utils::log::Level::error, __VA_ARGS__)
#define NX_WARNING(...) NX_UTILS_LOG(nx::utils::log::Level::warning, __VA_ARGS__)
#define NX_INFO(...) NX_UTILS_LOG(nx::utils::log::Level::info, __VA_ARGS__)
#define NX_DEBUG(...) NX_UTILS_LOG(nx::utils::log::Level::debug, __VA_ARGS__)
#define NX_VERBOSE(...) NX_UTILS_LOG(nx::utils::log::Level::verbose, __VA_ARGS__)

/** Use as a logging tag in functions without "this". */
#define NX_SCOPE_TAG nx::utils::log::Tag(scopeOfFunction(typeid(ScopeTag), __FUNCTION__))

} // namespace log
} // namespace utils
} // namespace nx
