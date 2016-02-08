
#include "multiserver_statistics_rest_handler.h"

#include <network/tcp_listener.h>
#include <common/common_module.h>
#include <utils/common/model_functions.h>
#include <api/helpers/empty_request_data.h>
#include <rest/helpers/request_context.h>
#include <rest/helpers/request_helpers.h>
#include <rest/server/rest_connection_processor.h>
#include <core/resource/media_server_resource.h>
#include <core/resource_management/resource_pool.h>

#include <statistics/base_statistics_settings_loader.h>

namespace
{
    const auto kFullUrlTemplate = lit("/%1/%2");

    template<typename HandlerType>
    StatisticsActionHandlerPtr createHandler(const QString &basePath)
    {
        return StatisticsActionHandlerPtr(new HandlerType(basePath));
    }

    template<typename HandlerType, typename ContextType>
    QUrl getServerApiUrl(const QString &basePath
        , const QnMediaServerResourcePtr &server
        , const ContextType &context)
    {
        QUrl result(server->getApiUrl());
        result.setPath(kFullUrlTemplate.arg(basePath, HandlerType::kHandlerPath));

        auto modifiedRequest = context.request();
        modifiedRequest.makeLocal();
        result.setQuery(modifiedRequest.toUrlQuery());

        return result;
    }

    bool hasInternetConnection(const QnMediaServerResourcePtr &server)
    {
        const auto flags = server->getServerFlags();
        return flags.testFlag(Qn::SF_HasPublicIP);
    }
}

class StatisticsActionHandler
{
public:
    StatisticsActionHandler(const QString &baseUrl);

    virtual ~StatisticsActionHandler();

    const QString &basePath() const;

    virtual int executeGet(const QnRequestParamList& params
        , QByteArray& result, QByteArray& contentType
        , int port) = 0;

private:
    const QString m_baseUrl;
};

StatisticsActionHandler::StatisticsActionHandler(const QString &baseUrl)
    : m_baseUrl(baseUrl)
{}

StatisticsActionHandler::~StatisticsActionHandler()
{}

const QString &StatisticsActionHandler::basePath() const
{
    return m_baseUrl;
}

//

class SettingsActionHandler : public StatisticsActionHandler
{
    typedef StatisticsActionHandler base_type;

public:
    static const QString kHandlerPath;

    SettingsActionHandler(const QString &baseUrl);

    virtual ~SettingsActionHandler();

    int executeGet(const QnRequestParamList& params
        , QByteArray& result, QByteArray& contentType
        , int port) override;

private:
    nx_http::StatusCode::Value loadSettingsLocally(QnStatisticsSettings &outputSettings);

    typedef QnMultiserverRequestContext<QnEmptyRequestData> Context;
    nx_http::StatusCode::Value loadSettingsRemotely(QnStatisticsSettings &outputSettings
        , const QnMediaServerResourcePtr &server
        , Context *context);

private:
};

const QString SettingsActionHandler::kHandlerPath = lit("settings");

SettingsActionHandler::SettingsActionHandler(const QString &baseUrl)
    : base_type(baseUrl)
{}

SettingsActionHandler::~SettingsActionHandler()
{}

int SettingsActionHandler::executeGet(const QnRequestParamList& params
    , QByteArray& result, QByteArray& contentType
    , int port)
{
    Context context(QnEmptyRequestData(), port);

    const auto &request = context.request();

    QnStatisticsSettings settings;
    nx_http::StatusCode::Value resultCode = nx_http::StatusCode::noContent;
    if (request.isLocal)
    {
        resultCode = loadSettingsLocally(settings);
    }
    else
    {
        // Otherwise we load settings from other available servers
        const auto moduleGuid = qnCommon->moduleGUID();
        for (const auto server: qnResPool->getAllServers(Qn::Online))
        {
            resultCode = (server->getId() == moduleGuid
                ? loadSettingsLocally(settings)
                : loadSettingsRemotely(settings, server, &context));

            if (resultCode == nx_http::StatusCode::ok)
                break;
        }
    }

    if (resultCode != nx_http::StatusCode::ok)
        return resultCode;

    QnFusionRestHandlerDetail::serialize(settings, result, contentType
        , request.format, request.extraFormatting);
    return nx_http::StatusCode::ok;
}

nx_http::StatusCode::Value SettingsActionHandler::loadSettingsLocally(QnStatisticsSettings &outputSettings)
{
    // At first we try to load settings through the current server
    const auto moduleGuid = qnCommon->moduleGUID();
    const auto server = qnResPool->getResourceById<QnMediaServerResource>(moduleGuid);
    if (!server || !hasInternetConnection(server))
        return nx_http::StatusCode::noContent;

    // TODO: implement me!
    return nx_http::StatusCode::ok;
}

nx_http::StatusCode::Value SettingsActionHandler::loadSettingsRemotely(
    QnStatisticsSettings &outputSettings
    , const QnMediaServerResourcePtr& server
    , Context *context)
{
    auto result = nx_http::StatusCode::noContent;
    if (!hasInternetConnection(server))
        return result;

    const auto completionFunc = [&outputSettings, &result, context]
        (SystemError::ErrorCode osErrorCode, int statusCode, nx_http::BufferType body)
    {
        const auto httpCode = static_cast<nx_http::StatusCode::Value>(statusCode);

        QnStatisticsSettings settings;
        bool success = false;
        if ((osErrorCode == SystemError::noError)
            && (httpCode == nx_http::StatusCode::ok))
        {
            settings = QnUbjson::deserialized(body, settings, &success);
        }

        const auto updateOutputDataCallback =
            [&outputSettings, &result, success, settings, context, httpCode]()
        {
            if (success)
            {
                outputSettings = settings;
                result = httpCode;
            }

            context->requestProcessed();
        };

        context->executeGuarded(updateOutputDataCallback);
    };

    const QUrl apiUrl = getServerApiUrl<SettingsActionHandler>(basePath(), server, *context);
    runMultiserverDownloadRequest(apiUrl, server, completionFunc, context);
    context->waitForDone();
    return result;
}

//

QnMultiserverStatisticsRestHandler::QnMultiserverStatisticsRestHandler(const QString &basePath)
    : QnFusionRestHandler()
{
    m_handlers.insert(kFullUrlTemplate.arg(basePath, SettingsActionHandler::kHandlerPath)
        , createHandler<SettingsActionHandler>(basePath));
}

QnMultiserverStatisticsRestHandler::~QnMultiserverStatisticsRestHandler()
{}

int QnMultiserverStatisticsRestHandler::executeGet(const QString& path
    , const QnRequestParamList& params
    , QByteArray& result
    , QByteArray& contentType
    , const QnRestConnectionProcessor *processor)
{
    const auto handlerIt = m_handlers.find(path);
    if (handlerIt == m_handlers.end())
        return nx_http::StatusCode::notFound;

    const auto handler = handlerIt.value();
    return handler->executeGet(params, result, contentType
        , processor->owner()->getPort());
}
