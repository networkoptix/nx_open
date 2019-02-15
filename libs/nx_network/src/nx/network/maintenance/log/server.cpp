#include "server.h"

#include <nx/fusion/model_functions.h>
#include <nx/network/http/buffer_source.h>
#include <nx/network/url/url_parse_helper.h>
#include <nx/network/http/writable_message_body.h>
#include <nx/utils/log/logger_builder.h>
#include <nx/utils/log/logger_collection.h>

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

    for (const auto& element : loggersByFilters)
        uniqueLoggers.emplace(element.second);

    return uniqueLoggers;
}

} // namespace

Server::Server(LoggerCollection * loggerCollection /*= nullptr*/):
    m_loggerCollection(loggerCollection)
{
    if (!m_loggerCollection)
        m_loggerCollection = LoggerCollection::instance();
}

void Server::registerRequestHandlers(
    const std::string& basePath,
    http::server::rest::MessageDispatcher* messageDispatcher)
{
    messageDispatcher->registerRequestProcessorFunc(
        http::Method::get,
        url::joinPath(basePath, kLoggers),
        [this](auto&&... args) { serveGetLoggers(std::move(args)...); });

    messageDispatcher->registerRequestProcessorFunc(
        http::Method::delete_,
        url::joinPath(basePath, kLoggers, "/{id}"),
        [this](auto&&... args) { serveDeleteLogger(std::move(args)...); });

    messageDispatcher->registerRequestProcessorFunc(
        http::Method::post,
        url::joinPath(basePath, kLoggers),
        [this](auto&&... args) { servePostLogger(std::move(args)...); });

    messageDispatcher->registerRequestProcessorFunc(
        http::Method::get,
        url::joinPath(basePath, kStream),
        [this](auto&&... args) { serveGetStreamingLogger(std::move(args)...); });
}

void Server::serveGetLoggers(
    http::RequestContext /*requestContext*/,
    http::RequestProcessedHandler completionHandler)
{
    auto uniqueLoggers = removeDuplicates(m_loggerCollection->allLoggers());

    Loggers loggers;
    for (const auto& loggerContext : uniqueLoggers)
    {
        loggers.loggers.push_back(utils::toLoggerInfo(
            loggerContext.logger,
            m_loggerCollection->getEffectiveFilters(loggerContext.id),
            loggerContext.id));
    }

    http::RequestResult result(http::StatusCode::ok);
    result.dataSource = std::make_unique<http::BufferSource>(
        "application/json",
        QJson::serialized(loggers));

    completionHandler(std::move(result));
}

void Server::serveDeleteLogger(
    http::RequestContext requestContext,
    http::RequestProcessedHandler completionHandler)
{
    int loggerId = LoggerCollection::kInvalidId;
    try
    {
        loggerId = std::stoi(requestContext.requestPathParams.getByName("id"));
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
    bool ok = false;
    Logger newLoggerInfo = QJson::deserialized(requestContext.request.messageBody, Logger(), &ok);
    if (!ok)
        return completionHandler(http::StatusCode::badRequest);

    Settings logSettings;
    logSettings.loggers.push_back(utils::toLoggerSettings(newLoggerInfo));

    // TODO: #nw I'd recommend to correctly fill logSettings.levelFilters instead of using
    // ::tofilters here and pass raw filters to the logger.
    auto newLogger = LoggerBuilder::buildLogger(
        logSettings,
        QString(), //< TODO: #nw get the application name
        QString(), //< TODO: #nw get the appliation binary path
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
        logger,
        m_loggerCollection->getEffectiveFilters(loggerId),
        loggerId);

    http::RequestResult result(http::StatusCode::created);
    result.dataSource = std::make_unique<http::BufferSource>(
        "application/json",
        QJson::serialized(loggerInfo));

    completionHandler(std::move(result));
}

void Server::serveGetStreamingLogger(
    http::RequestContext requestContext,
    http::RequestProcessedHandler completionHandler)
{
    LoggerSettings loggerSettings;
    if (!loggerSettings.parse(requestContext.request.requestLine.url.query()))
        return completionHandler(http::StatusCode::badRequest);

    auto messageBody = std::make_unique<WritableMessageBody>("text/plain");
    auto logWriter = std::make_unique<StreamingLogWriter>(messageBody.get());
    auto logWriterPtr = logWriter.get();

    Settings logSettings;
    logSettings.loggers.push_back(loggerSettings);

    // TODO: #nw I'd recommend to correctly fill logSettings.levelFilters instead of using
    // ::tofilters here and pass raw filters to the logger.
    auto newLogger = LoggerBuilder::buildLogger(
        logSettings,
        QString(), //< TODO: #nw get the application name
        QString(), //< TODO: #nw get the appliation binary path
        utils::toFilters(loggerSettings.level.filters),
        std::move(logWriter));
    if (!newLogger)
        return completionHandler(http::StatusCode::internalServerError);

    std::shared_ptr<AbstractLogger> sharedNewLogger(std::move(newLogger));

    int loggerId = m_loggerCollection->add(sharedNewLogger);
    if (loggerId == LoggerCollection::kInvalidId)
        return completionHandler(http::RequestResult(http::StatusCode::badRequest));

    messageBody->setOnBeforeDestructionHandler(
        [this, sharedNewLogger, logWriterPtr, loggerId]()
        {
            logWriterPtr->setMessageBody(nullptr);
            m_loggerCollection->remove(loggerId);
        });

    http::RequestResult result(http::StatusCode::ok);
    result.dataSource = std::move(messageBody);

    completionHandler(std::move(result));
}

} // namespace nx::network::maintenance::log
