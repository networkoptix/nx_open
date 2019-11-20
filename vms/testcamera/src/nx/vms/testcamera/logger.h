#pragma once

#include <chrono>
#include <vector>

#include <QtCore/QString>
#include <QtCore/QDateTime>

#include <nx/utils/scope_guard.h>
#include <nx/utils/log/log.h>

namespace nx::vms::testcamera {

/**
 * Keeps the logging tag and a context for log messages. Intended to be used with the accompanying
 * macros.
 */
class Logger final
{
public:
    Logger(const Logger&) = delete;
    Logger(Logger&&) = delete;
    Logger& operator=(const Logger&) = delete;
    Logger& operator=(Logger&&) = delete;

    /**
     * @param tag Contains a text to appear at the beginning of each message. Should not
     *     be in brackets or have trailing punctuation or whitespace.
     */
    Logger(QString tag): m_tag(std::move(tag)) {}

    QString tag() const { return m_tag; }

    /**
     * Pushes the specified suffix to the logging context - the stack of suffixes for all future
     * log messages. When the returned object is destroyed, the suffix is popped back from the
     * stack, restoring the previous logging context.
     */
    nx::utils::SharedGuard pushContext(const QString& contextCaption);

    /**
     * Creates a copy of this object (that will have its own logging context) and pushes the
     * specified context caption into this copy.
     */
    std::unique_ptr<Logger> cloneForContext(const QString& contextCaption) const;

    /**
     * @return Concatenation of all pushed context captions via comma-and-space, with the
     *     necessary leading whitespace and prefix, intended to be appended to the logged messages.
     */
    QString context() const { return m_context; }

private:
    void pushContextCaption(const QString& contextCaption);
    void popContextCaption(const QString& contextCaption);
    void updateContext();

private:
    const QString m_tag;
    std::vector<QString> m_contextCaptions;
    QString m_context;
};

QString us(std::chrono::microseconds value);
QString us(int64_t valueUs);

/**
 * Logs the message with its optional %N-substituted ARGS using NX_UTILS_LOG; the logging tag is
 * taken from LOGGER, and LOGGER->context() is appended to the message.
 */
#define NX_LOGGER_LOG(LOGGER, LEVEL, /* MESSAGE[, ARGS...] */ ...) \
    NX_MSVC_EXPAND( \
        NX_GET_13TH_ARG( /*< Calling one of the two macros depending on ARGS presence. */ \
            __VA_ARGS__, \
            /* Repeated 11 times: allow up to 11 args after MESSAGE, as NX_UTILS_LOG() does. */ \
            NX_LOGGER_DETAIL_LOG_4_OR_MORE_ARGS, \
            NX_LOGGER_DETAIL_LOG_4_OR_MORE_ARGS, \
            NX_LOGGER_DETAIL_LOG_4_OR_MORE_ARGS, \
            NX_LOGGER_DETAIL_LOG_4_OR_MORE_ARGS, \
            NX_LOGGER_DETAIL_LOG_4_OR_MORE_ARGS, \
            NX_LOGGER_DETAIL_LOG_4_OR_MORE_ARGS, \
            NX_LOGGER_DETAIL_LOG_4_OR_MORE_ARGS, \
            NX_LOGGER_DETAIL_LOG_4_OR_MORE_ARGS, \
            NX_LOGGER_DETAIL_LOG_4_OR_MORE_ARGS, \
            NX_LOGGER_DETAIL_LOG_4_OR_MORE_ARGS, \
            NX_LOGGER_DETAIL_LOG_4_OR_MORE_ARGS, \
            NX_LOGGER_DETAIL_LOG_3_ARGS, \
            not_enough_args, not_enough_args, not_enough_args /* Chosen if less than 3 args. */ \
        )(LOGGER, LEVEL, __VA_ARGS__) \
    )

#define NX_LOGGER_ERROR(LOGGER, ...) \
    NX_LOGGER_LOG(LOGGER, nx::utils::log::Level::error, __VA_ARGS__)

#define NX_LOGGER_WARNING(LOGGER, ...) \
    NX_LOGGER_LOG(LOGGER, nx::utils::log::Level::warning, __VA_ARGS__)

#define NX_LOGGER_INFO(LOGGER, ...) \
    NX_LOGGER_LOG(LOGGER, nx::utils::log::Level::info, __VA_ARGS__)

#define NX_LOGGER_DEBUG(LOGGER, ...) \
    NX_LOGGER_LOG(LOGGER, nx::utils::log::Level::debug, __VA_ARGS__)

#define NX_LOGGER_VERBOSE(LOGGER, ...) \
    NX_LOGGER_LOG(LOGGER, nx::utils::log::Level::verbose, __VA_ARGS__)

//-------------------------------------------------------------------------------------------------
// private

#define NX_LOGGER_DETAIL_ARGS_FOR_NX_UTILS_LOG(LOGGER, LEVEL, MESSAGE) \
    LEVEL, nx::utils::log::Tag(LOGGER->tag()), MESSAGE + LOGGER->context()

#define NX_LOGGER_DETAIL_LOG_3_ARGS(LOGGER, LEVEL, MESSAGE) \
    NX_UTILS_LOG(NX_LOGGER_DETAIL_ARGS_FOR_NX_UTILS_LOG(LOGGER, LEVEL, MESSAGE))

#define NX_LOGGER_DETAIL_LOG_4_OR_MORE_ARGS(LOGGER, LEVEL, MESSAGE, ...) \
    NX_UTILS_LOG(NX_LOGGER_DETAIL_ARGS_FOR_NX_UTILS_LOG(LOGGER, LEVEL, MESSAGE), __VA_ARGS__)

} // namespace nx::vms::testcamera
