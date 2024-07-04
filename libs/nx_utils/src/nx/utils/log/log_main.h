// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <nx/utils/system_error.h>
#include <nx/utils/time.h>

#include "log_logger.h"

namespace nx::log {

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

class LevelReducer
{
public:
    LevelReducer(Level level): m_level(level) {}
    Level baseLevel() const { return m_level; }

    std::pair<Level, bool /*isOnLimit*/> nextLevel()
    {
        if (!s_isEnabled || (m_level > Level::info))
            return {m_level, /*isOnLimit*/ false};

        const auto limit = (uint32_t) nx::utils::ini().logLevelReducerPassLimit;
        const auto windowSizeS = (uint32_t) nx::utils::ini().logLevelReducerWindowSizeS;

        const auto nowS = (uint32_t) std::chrono::duration_cast<std::chrono::seconds>(
            nx::utils::monotonicTime().time_since_epoch()).count();

        const auto windowStartS = m_windowStartS.load();
        if (m_passCount == 0 || windowStartS + windowSizeS <= nowS || windowStartS > nowS)
        {
            // It is possible to have a race on these 2 atomics and actual log writing, but it
            // would only affect the number of records on each level and their order.
            m_windowStartS = nowS;
            m_passCount = 0;
        }

        const auto newCount = ++m_passCount;
        return {
            (newCount <= limit) ? m_level : Level::debug,
            /*isOnLimit*/ (newCount == limit)
        };
    }

    static void setEnabled(bool value) { s_isEnabled = value; }

private:
    NX_UTILS_API static bool s_isEnabled /*= true*/;

    const Level m_level;
    std::atomic<uint32_t> m_passCount{0};
    std::atomic<uint32_t> m_windowStartS{0};
};

class Helper
{
public:
    Helper() {} //< Constructing a helper which does not log anything.

    Helper(LevelReducer* levelReducer, Tag tag):
        m_tag(std::move(tag)),
        m_levelReducer(levelReducer),
        m_logger(getLogger(m_tag))
    {
        if (m_logger && !m_logger->isToBeLogged(levelReducer->baseLevel(), m_tag))
            m_logger.reset();
    }

    void log(const QString& message) const
    {
        // Avoid crashing during the static deinitialization phase - log to stderr instead.
        if (!m_logger)
        {
            // LevelReducer can be already destroyed at this time, so ignore the level.
            std::cerr << (m_tag.toString() + ": " + message + "\n").toStdString();
            std::flush(std::cerr);
            return;
        }

        const auto [level, isOnLimit] = m_levelReducer->nextLevel();
        m_logger->log(level, m_tag, isOnLimit ? "TOO MANY SIMILAR MESSAGES: " + message : message);
    }

    explicit operator bool() const { return m_logger.get(); }

protected:
   const Tag m_tag;
   LevelReducer* const m_levelReducer = nullptr;
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
        m_strings << nx::toString(value);
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

} // namespace detail

inline void setLevelReducerEnabled(bool value)
{
    detail::LevelReducer::setEnabled(value);
}

/**
 * NOTE: Preserves current system error code (SystemError::getLastOSErrorCode()).
 */
#define NX_UTILS_LOG_MESSAGE(LEVEL, TAG, ...) do \
{ \
    struct ScopeTag{}; /*< Used by NX_SCOPE_TAG to get scope from demangled type_info::name(). */ \
    if ((LEVEL) <= nx::log::maxLevel()) \
    { \
        const auto systemErrorBak = SystemError::getLastOSErrorCode(); \
        static nx::log::detail::LevelReducer levelReducer(LEVEL); \
        if (auto _log_helper = nx::log::detail::Helper(&levelReducer, (TAG))) \
            _log_helper.log(::nx::format(__VA_ARGS__)); \
        SystemError::setLastErrorCode(systemErrorBak); \
    } \
} while (0)

#define NX_UTILS_LOG_STREAM(LEVEL, TAG) \
    for (auto stream = \
            [&]() \
            { \
                if ((LEVEL) > nx::log::maxLevel()) \
                    return nx::log::detail::Stream(); \
                static nx::log::detail::LevelReducer levelReducer(LEVEL); \
                return nx::log::detail::Stream(&levelReducer, (TAG));  \
            }(); \
        stream; stream.flush()) \
            stream /* <<... */

/**
 * Can be used to redirect nx_kit's NX_PRINT to log as following:
 * <pre><code>
 *     #define NX_PRINT NX_UTILS_LOG_STREAM_NO_SPACE( \
 *         nx::log::Level::debug, \
 *         nx::log::Tag(QStringLiteral("my_plugin")) \
 *     ) << NX_PRINT_PREFIX
 * </code></pre>
 */
#define NX_UTILS_LOG_STREAM_NO_SPACE(LEVEL, TAG) \
    NX_UTILS_LOG_STREAM(LEVEL, TAG).setDelimiter(QString()) /* <<... */

/**
 * Usage:
 *     NX_<LEVEL>(TAG, MESSAGE [, VALUES...]; //< Writes MESSAGE to log if LEVEL and TAG allow.
 * or, when the log level is known only at runtime:
 *     NX_UTILS_LOG(nx::log::Level::<level>, TAG, MESSAGE [, VALUES...];
 *
 * Examples:
 *     NX_INFO(this, "Expected value %1", value);
 *     NX_DEBUG(NX_SCOPE_TAG, "Message from a non-member function.");
 *
 * Deprecated usage:
 *     NX_ERROR(this) << "Unexpected value" << value;
 */
#define NX_UTILS_LOG(...) \
    /* Choose one of the two macros depending on the number of args supplied to this macro. */ \
    NX_MSVC_EXPAND(NX_GET_15TH_ARG( \
        __VA_ARGS__, \
        /* Repeat 12 times: allow for up to 11 args which are %N-substituted into the message. */ \
        NX_UTILS_LOG_MESSAGE, NX_UTILS_LOG_MESSAGE, NX_UTILS_LOG_MESSAGE, NX_UTILS_LOG_MESSAGE, \
        NX_UTILS_LOG_MESSAGE, NX_UTILS_LOG_MESSAGE, NX_UTILS_LOG_MESSAGE, NX_UTILS_LOG_MESSAGE, \
        NX_UTILS_LOG_MESSAGE, NX_UTILS_LOG_MESSAGE, NX_UTILS_LOG_MESSAGE, NX_UTILS_LOG_MESSAGE, \
        NX_UTILS_LOG_STREAM, /*< Chosen when called with TAG arg only (without a message). */ \
        args_required /*< Chosen when called without arguments; leads to an error. */ \
    )(__VA_ARGS__))

/**
 * Prints a critical error that is likely to cause a complete service interruption. The message
 * text must be clear and make sense to the end-user.
 *
 * Example:
 * <pre><code>
 *     NX_ERROR(this, "Connection to the DB has been lost: %1", err);
 * </code></pre>
 */
#define NX_ERROR(...) NX_UTILS_LOG(nx::log::Level::error, __VA_ARGS__)

/**
 * Prints a non-critical error/warning that may cause issues with limited impact. The message text
 * must be clear and make sense to the end-user.
 *
 * Example:
 * <pre><code>
 *     NX_WARNING(this, "Camera %1 went offline", camera->id());
 * </code></pre>
 */
#define NX_WARNING(...) NX_UTILS_LOG(nx::log::Level::warning, __VA_ARGS__)

/**
 * Prints an information message about the service's state. The number of such messages must not be
 * large regardless of the load. The message text must be clear and make sense to the end-user.
 *
 * Example:
 * <pre><code>
 *     NX_INFO(this, "Bound to https port %1 and listening", server->port());
 * </code></pre>
 */
#define NX_INFO(...) NX_UTILS_LOG(nx::log::Level::info, __VA_ARGS__)

/**
 * Prints a message that is useful for understanding how a service is operating. Some examples are:
 * user authentication result, API request processing result. Bad examples are: tracing details of
 * a request processing, logging the network traffic.
 *
 * The message should be understandable by a QA/support person, but may also contain information
 * targeted solely for developers.
 *
 * There should not be too many of such messages (though, it may depend on the load). Generally,
 * it should be possible to enable these messages in a production environment for a prolonged
 * period of time without an impact on the service.
 *
 * Example:
 * <pre><code>
 *     NX_DEBUG(this, "User %1 added to a system %2 with role %3", username, systemId, role);
 * </code></pre>
 */
#define NX_DEBUG(...) NX_UTILS_LOG(nx::log::Level::debug, __VA_ARGS__)

/**
 * Prints a message that is helpful to a developer for understanding the details of a service
 * behavior.
 *
 * A good example is logging the details of an HTTP request/response exchange:
 * - "connection to %1 is established"
 * - "request headers were sent, sending body"
 * - "full message was sent, waiting for reply"
 *
 * The number of messages at this log level may be huge. It must be expected that enabling this log
 * level on a production system will have a significant impact on the system performance. It is
 * recommended to enable these messages for a short period, only while providing a targeted message
 * filter.
 *
 * Example:
 * <pre><code>
 *     NX_VERBOSE(this, "%1 response headers from %2 have been received. Waiting for a body",
 *         response.headers.size(), connection->getForeignAddress());
 * </code></pre>
 */
#define NX_VERBOSE(...) NX_UTILS_LOG(nx::log::Level::verbose, __VA_ARGS__)

/**
 * Prints a message with very low-level information.
 *
 * The message is likely to make sense only to those developers who are familiar with the given
 * block of code. Can be used to trace an execution where the debugger is not available.
 *
 * It is recommended to use this for messages in huge quantities that can be useful rarely to
 * certain developers only. An example is tracing an SSL socket: logging each read, write,
 * encryption, decryption.
 *
 * The `trace` log level can only be enabled with a non-empty filter. The filter should be limited
 * to the specific class or a small namespace.
 *
 * The number of log messages is likely to be huge in a production environment. It must be expected
 * that enabling this level even with a narrow filter will have a negative impact on the
 * performance.
 *
 * If you decide to use this log level, do not expect that someone else will collect these logs for
 * you or analyze them.
 *
 * Example:
 * <pre><code>
 *     NX_TRACE(this, "Read %1 bytes out of %2 requested", result, size);
 * </code></pre>
 */
#define NX_TRACE(...) NX_UTILS_LOG(nx::log::Level::trace, __VA_ARGS__)

/** Use as a logging tag in functions without "this". */
#define NX_SCOPE_TAG nx::log::Tag(nx::scopeOfFunction(typeid(ScopeTag), __FUNCTION__))

} // namespace nx::log
