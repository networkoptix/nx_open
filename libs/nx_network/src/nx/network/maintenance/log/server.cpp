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
        [this](auto&&... args) { serveDeleteLoggers(std::move(args)...); });

    messageDispatcher->registerRequestProcessorFunc(
        http::Method::post,
        url::joinPath(basePath, kLoggers),
        [this](auto&&... args) { servePostLoggers(std::move(args)...); });
}

void Server::serveGetLoggers(
    http::RequestContext /*requestContext*/,
    http::RequestProcessedHandler completionHandler)
{
    Loggers loggers;

    auto uniqueLoggers = removeDuplicates(m_loggerCollection->allLoggers());

    for (const auto& loggerContext : uniqueLoggers)
    {
        loggers.loggers.push_back(utils::toLoggerInfo(
            loggerContext.logger,
            m_loggerCollection->getEffectiveTags(loggerContext.id),
            loggerContext.id));
    }

    http::RequestResult result(http::StatusCode::ok);
    result.dataSource = std::make_unique<http::BufferSource>(
        kApplicationType,
        QJson::serialized(loggers));

    completionHandler(std::move(result));
}

void Server::serveDeleteLoggers(
    http::RequestContext requestContext,
    http::RequestProcessedHandler completionHandler)
{
    int loggerId = LoggerCollection::invalidId;
    try
    {
        loggerId = std::stoi(requestContext.requestPathParams.getByName("id"));
    }
    catch (const std::exception& e)
    {
        return completionHandler(http::RequestResult(http::StatusCode::badRequest));
    }
    
    if (!m_loggerCollection->get(loggerId))
        return completionHandler(http::RequestResult(http::StatusCode::notFound));

    m_loggerCollection->remove(loggerId);

    completionHandler(http::RequestResult(http::StatusCode::ok));
}

void Server::servePostLoggers(
    http::RequestContext requestContext,
    http::RequestProcessedHandler completionHandler)
{
    Logger newLoggerInfo = QJson::deserialized<Logger>(requestContext.request.messageBody);

    Settings logSettings;
    LoggerSettings loggerSettings;

    File::Settings fileSettings;
    fileSettings.size = loggerSettings.maxFileSize;
    fileSettings.count = loggerSettings.maxBackupCount;
    fileSettings.name = QString::fromStdString(newLoggerInfo.path);

    logSettings.loggers.push_back(loggerSettings);

    auto newLogger = LoggerBuilder().buildLogger(
        logSettings,
        QString(), // TODO: #nw get the application name
        QString(), // TODO: #nw get the appliation binary path
        utils::toTags(newLoggerInfo.filters),
        std::make_unique<File>(fileSettings));

    int id = m_loggerCollection->add(std::move(newLogger));
    if (id == LoggerCollection::invalidId)
        return completionHandler(http::RequestResult(http::StatusCode::badRequest));

    auto logger = m_loggerCollection->get(id);
    if (!logger)
        return completionHandler(http::RequestResult(http::StatusCode::badRequest));

    Logger loggerInfo = utils::toLoggerInfo(
        logger,
        m_loggerCollection->getEffectiveTags(id),
        id);
    
    http::RequestResult result(http::StatusCode::created);
    result.dataSource = std::make_unique<http::BufferSource>(
        kApplicationType,
        QJson::serialized(loggerInfo));
    
    completionHandler(std::move(result));
}

} // namespace nx::network::maintenance::log
