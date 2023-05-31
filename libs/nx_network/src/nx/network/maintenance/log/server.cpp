// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "server.h"

#include <nx/network/http/buffer_source.h>
#include <nx/network/url/url_parse_helper.h>
#include <nx/network/http/writable_message_body.h>
#include <nx/reflect/json.h>
#include <nx/utils/log/logger_builder.h>
#include <nx/utils/log/logger_collection.h>
#include <nx/utils/log/aggregate_logger.h>

#include "utils.h"
#include "logger.h"
#include "request_path.h"
#include "streaming_log_writer.h"

namespace nx::network::maintenance::log {

using namespace nx::utils::log;
using namespace nx::network::http;

namespace {

std::set<LoggerCollection::Context> removeDuplicates(
    const LoggerCollection::LoggersByFilter& loggersByFilters)
{
    std::set<LoggerCollection::Context> uniqueLoggers;

    for (const auto& element: loggersByFilters)
        uniqueLoggers.emplace(element.second);

    return uniqueLoggers;
}

} // namespace

Server::Server(LoggerCollection* loggerCollection /*= nullptr*/):
    m_loggerCollection(loggerCollection)
{
    if (!m_loggerCollection)
        m_loggerCollection = LoggerCollection::instance();
}

void Server::registerRequestHandlers(
    const std::string& basePath,
    http::server::rest::MessageDispatcher* messageDispatcher)
{
    // NOTE: #akolesnikov The API docs assume that these functions are registered under
    // /placeholder/maintenance/log prefix, which is usually true but not guaranteed.

    /**%apidoc GET /placeholder/maintenance/log/loggers
     * Retrieves the list of the current Loggers.
     * %caption Get Loggers
     * %ingroup Maintenance
     * %return List of the current Loggers.
     */
    messageDispatcher->registerRequestProcessorFunc(
        http::Method::get,
        url::joinPath(basePath, kLoggers),
        [this](auto&&... args) { serveGetLoggers(std::move(args)...); });

    /**%apidoc DELETE /placeholder/maintenance/log/loggers/{id}
     * Deletes the specified Logger.
     * %caption Delete Logger
     * %param id Id of a Logger as reported by the .../loggers call.
     * %ingroup Maintenance
     */
    messageDispatcher->registerRequestProcessorFunc(
        http::Method::delete_,
        url::joinPath(basePath, kLoggers, "/{id}"),
        [this](auto&&... args) { serveDeleteLogger(std::move(args)...); });

    /**%apidoc POST /placeholder/maintenance/log/loggers
     * Adds a new Logger.
     * %caption Add Logger
     * %ingroup Maintenance
     */
    messageDispatcher->registerRequestProcessorFunc(
        http::Method::post,
        url::joinPath(basePath, kLoggers),
        [this](auto&&... args) { servePostLogger(std::move(args)...); });

    /**%apidoc POST /placeholder/maintenance/log/stream
     * Starts sending log messages that satisfy the given filter. The log stream is closed when the
     * TCP connection is closed by the Client. The request requires that a filter is specified.
     * %caption Get log stream
     * %ingroup Maintenance
     * %param level Example: "level[message_prefix]". E.g., "level=verbose[nx::cloud::relay]"
     * %return Stream with the log messages in the text/plain format.
     */
    messageDispatcher->registerRequestProcessorFunc(
        http::Method::get,
        url::joinPath(basePath, kStream),
        [this](auto&&... args) { serveGetStreamingLogger(std::move(args)...); });
}

void Server::serveGetLoggers(
    http::RequestContext /*requestContext*/,
    http::RequestProcessedHandler completionHandler)
{
    http::RequestResult result(http::StatusCode::ok);
    result.body = std::make_unique<http::BufferSource>(
        "application/json",
        nx::Buffer(nx::reflect::json::serialize(mergeLoggers())));

    completionHandler(std::move(result));
}

void Server::serveDeleteLogger(
    http::RequestContext requestContext,
    http::RequestProcessedHandler completionHandler)
{
    int loggerId = LoggerCollection::kInvalidId;
    try
    {
        loggerId = nx::utils::stoi(requestContext.requestPathParams.getByName("id"));
    }
    catch (const std::exception&)
    {
        return completionHandler(http::StatusCode::badRequest);
    }

    if (!m_loggerCollection->get(loggerId))
        return completionHandler(http::RequestResult(http::StatusCode::notFound));

    m_loggerCollection->remove(loggerId);

    completionHandler(http::RequestResult(http::StatusCode::ok));
}

void Server::servePostLogger(
    http::RequestContext requestContext,
    http::RequestProcessedHandler completionHandler)
{
    const auto [newLoggerInfo, deserializationResult] =
        nx::reflect::json::deserialize<Logger>(requestContext.request.messageBody);
    if (!deserializationResult.success)
        return completionHandler(http::StatusCode::badRequest);

    Settings logSettings;
    logSettings.loggers.push_back(utils::toLoggerSettings(newLoggerInfo));

    // TODO: #nw I'd recommend to correctly fill logSettings.levelFilters instead of using
    // ::tofilters here and pass raw filters to the logger.
    auto newLogger = LoggerBuilder::buildLogger(
        logSettings,
        "", //< TODO: #nw get the application name
        "", //< TODO: #nw get the application binary path
        utils::toFilters(newLoggerInfo.filters),
        nullptr);
    if (!newLogger)
        return completionHandler(http::StatusCode::internalServerError);

    int loggerId = m_loggerCollection->add(std::move(newLogger));
    if (loggerId == LoggerCollection::kInvalidId)
        return completionHandler(http::RequestResult(http::StatusCode::badRequest));

    auto logger = m_loggerCollection->get(loggerId);
    if (!logger)
        return completionHandler(http::RequestResult(http::StatusCode::internalServerError));

    Logger loggerInfo = utils::toLoggerInfo(
        logger.get(),
        m_loggerCollection->getEffectiveFilters(loggerId),
        loggerId);

    http::RequestResult result(http::StatusCode::created);
    result.body = std::make_unique<http::BufferSource>(
        "application/json",
        nx::Buffer(nx::reflect::json::serialize(loggerInfo)));

    completionHandler(std::move(result));
}

void Server::serveGetStreamingLogger(
    http::RequestContext requestContext,
    http::RequestProcessedHandler completionHandler)
{
    LoggerSettings loggerSettings;
    if (!loggerSettings.parse(requestContext.request.requestLine.url.query()))
    {
        NX_DEBUG(this, "Failed to parse log settings %1",
            requestContext.request.requestLine.url.query());
        return completionHandler(http::StatusCode::badRequest);
    }

    auto messageBody = std::make_unique<WritableMessageBody>("text/plain");
    auto logWriter = std::make_unique<StreamingLogWriter>(messageBody.get());
    auto logWriterPtr = logWriter.get();

    Settings logSettings;
    logSettings.loggers.push_back(loggerSettings);

    // TODO: #nw I'd recommend to correctly fill logSettings.levelFilters instead of using
    // ::tofilters here and pass raw filters to the logger.
    auto newLogger = LoggerBuilder::buildLogger(
        logSettings,
        "", //< TODO: #nw get the application name
        "", //< TODO: #nw get the application binary path
        utils::toFilters(loggerSettings.level.filters),
        std::move(logWriter));
    if (!newLogger)
    {
        NX_DEBUG(this, "Failed to create logger %1",
            requestContext.request.requestLine.url.query());
        return completionHandler(http::StatusCode::internalServerError);
    }

    std::shared_ptr<AbstractLogger> sharedNewLogger(std::move(newLogger));

    int loggerId = m_loggerCollection->add(sharedNewLogger);
    if (loggerId == LoggerCollection::kInvalidId)
    {
        NX_DEBUG(this, "Failed to install logger %1",
            requestContext.request.requestLine.url.query());
        return completionHandler(http::RequestResult(http::StatusCode::badRequest));
    }

    messageBody->setOnBeforeDestructionHandler(
        [this, sharedNewLogger, logWriterPtr, loggerId]()
        {
            logWriterPtr->setMessageBody(nullptr);
            m_loggerCollection->remove(loggerId);
        });

    http::RequestResult result(http::StatusCode::ok);
    result.body = std::move(messageBody);

    completionHandler(std::move(result));
}

Loggers Server::mergeLoggers() const
{
    Loggers allLoggersInfo;

    if (auto mainLogger = m_loggerCollection->main())
    {
        allLoggersInfo.loggers =
            splitAggregateLogger(
                LoggerCollection::kInvalidId,
                mainLogger.get(),
                mainLogger->filters());
    }

    auto allLoggers = removeDuplicates(m_loggerCollection->allLoggers());
    for (const auto& context : allLoggers)
    {
        auto aggregateLoggerInfo = splitAggregateLogger(
            context.id,
            context.logger.get(),
            m_loggerCollection->getEffectiveFilters(context.id));

        allLoggersInfo.loggers.insert(
            allLoggersInfo.loggers.end(),
            aggregateLoggerInfo.begin(),
            aggregateLoggerInfo.end());
    }

    return allLoggersInfo;
}

std::vector<Logger> Server::splitAggregateLogger(
    int id,
    nx::utils::log::AbstractLogger* logger,
    const std::set<nx::utils::log::Filter>& effectiveFilters) const
{
    auto aggregateLogger = dynamic_cast<AggregateLogger*>(logger);
    if (!aggregateLogger)
        return { utils::toLoggerInfo(logger, effectiveFilters, id) };

    std::vector<Logger> loggersInfo;
    const auto& actualLoggers = aggregateLogger->loggers();

    for (const auto& actualLogger : actualLoggers)
    {
        auto actualLoggerFilters = actualLogger->filters();
        for (
            auto it = actualLoggerFilters.begin();
            it != actualLoggerFilters.end();
            ++it)
        {
            if (effectiveFilters.find(*it) == effectiveFilters.end())
                it = actualLoggerFilters.erase(it);
        }

        loggersInfo.emplace_back(
            utils::toLoggerInfo(
                actualLogger.get(),
                actualLoggerFilters,
                id));
    }

    return loggersInfo;
};

} // namespace nx::network::maintenance::log
