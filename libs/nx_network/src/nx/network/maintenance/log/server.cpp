#include "server.h"

#include <nx/fusion/model_functions.h>
#include <nx/network/http/buffer_source.h>
#include <nx/network/url/url_parse_helper.h>            
#include <nx/utils/log/logger_builder.h>
#include <nx/utils/log/logger_collection.h>

#include "utils.h"
#include "logger.h"
#include "request_path.h"

namespace nx::network::maintenance::log {

using namespace nx::utils::log;

namespace {

static constexpr char kJsonMimeType[] = "application/json";

std::set<LoggerCollection::Context> removeDuplicates(
    const LoggerCollection::LoggersByTag& loggersByTag)
{
    std::set<LoggerCollection::Context> uniqueLoggers;

    for (const auto& element : loggersByTag)
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
            m_loggerCollection->getEffectiveTags(loggerContext.id),
            loggerContext.id));
    }

    http::RequestResult result(http::StatusCode::ok);
    result.dataSource = std::make_unique<http::BufferSource>(
        kJsonMimeType,
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
        return completionHandler(http::RequestResult(http::StatusCode::badRequest));
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

    auto newLogger = LoggerBuilder().buildLogger(
        logSettings,
        QString(), //< TODO: #nw get the application name
        QString(), //< TODO: #nw get the appliation binary path
        utils::toTags(newLoggerInfo.filters),
        nullptr);

    int id = m_loggerCollection->add(std::move(newLogger));
    if (id == LoggerCollection::kInvalidId)
        return completionHandler(http::RequestResult(http::StatusCode::badRequest));

    auto logger = m_loggerCollection->get(id);
    if (!logger)
        return completionHandler(http::RequestResult(http::StatusCode::internalServerError));

    Logger loggerInfo = utils::toLoggerInfo(
        logger,
        m_loggerCollection->getEffectiveTags(id),
        id);
    
    http::RequestResult result(http::StatusCode::created);
    result.dataSource = std::make_unique<http::BufferSource>(
        kJsonMimeType,
        QJson::serialized(loggerInfo));
    
    completionHandler(std::move(result));
}

} // namespace nx::network::maintenance::log
