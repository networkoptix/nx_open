
#include "multiserver_statistics_rest_handler.h"

#include <network/tcp_listener.h>
#include <common/common_module.h>
#include <utils/common/model_functions.h>
#include <utils/network/http/asynchttpclient.h>
#include <api/helpers/empty_request_data.h>
#include <api/helpers/send_statistics_request_data.h>
#include <rest/helpers/request_context.h>
#include <rest/helpers/request_helpers.h>
#include <rest/server/rest_connection_processor.h>
#include <core/resource/media_server_resource.h>
#include <core/resource_management/resource_pool.h>
#include <statistics/abstract_statistics_settings_loader.h>
#include <ec2_statictics_reporter.h>

namespace
{
    const auto kSendStatUser = lit("nx");
    const auto kSendStatPass = lit("f087996adb40eaed989b73e2d5a37c951f559956c44f6f8cdfb6f127ca4136cd");
    const nx_http::StringType kJsonContentType = Qn::serializationFormatToHttpContentType(Qn::JsonFormat);

    QString makeFullPath(const QString &basePath
        , const QString &postfix)
    {
        const auto kFullUrlTemplate = lit("/%1/%2");
        return kFullUrlTemplate.arg(basePath, postfix);
    }

    template<typename HandlerType>
    StatisticsActionHandlerPtr createHandler(const QString &basePath)
    {
        return StatisticsActionHandlerPtr(new HandlerType(basePath));
    }

    template<typename ContextType>
    QUrl getServerApiUrl(const QString &path
        , const QnMediaServerResourcePtr &server
        , const ContextType &context)
    {
        QUrl result(server->getApiUrl());
        result.setPath(path);

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

    bool isCorrectMetricsListJson(const QByteArray &body)
    {
        QnMetricHashesList dummy;
        return QJson::deserialize(body, &dummy);
    };
}

namespace
{
    class SettingsCache
    {
    public:
        static void update(const QnStatisticsSettings &settings);

        static void clear();

        static bool copyTo(QnStatisticsSettings &output);

    private:
        SettingsCache();

        static SettingsCache &instance();

    private:
        typedef QScopedPointer<QnStatisticsSettings> SettingsPtr;

        QnMutex m_mutex;
        QElapsedTimer m_timer;
        SettingsPtr m_settings;
    };

    SettingsCache::SettingsCache()
        : m_mutex()
        , m_timer()
        , m_settings()
    {
        m_timer.start();
    }

    SettingsCache &SettingsCache::instance()
    {
        static SettingsCache inst;
        return inst;
    }

    void SettingsCache::update(const QnStatisticsSettings &settings)
    {
        const QnMutexLocker guard(&instance().m_mutex);
        instance().m_timer.restart();
        instance().m_settings.reset(new QnStatisticsSettings(settings));
    }

    void SettingsCache::clear()
    {
        const QnMutexLocker guard(&instance().m_mutex);
        instance().m_timer.restart();
        instance().m_settings.reset();
    }

    bool SettingsCache::copyTo(QnStatisticsSettings &output)
    {
        const QnMutexLocker guard(&instance().m_mutex);

        enum { kMinSettingsUpdateTime = 4 * 60 * 60 * 1000 }; // every 4 hours

        auto &settings = instance().m_settings;
        auto &timer = instance().m_timer;
        if (!settings || timer.hasExpired(kMinSettingsUpdateTime))
            return false;

        output = *settings;
        return true;
    }
}

const QString QnMultiserverStatisticsRestHandler::kSettingsUrlParam = lit("clientStatisticsSettingsUrl");

class StatisticsActionHandler
{
public:
    StatisticsActionHandler(const QString &path);

    virtual ~StatisticsActionHandler();

    const QString &path() const;

    virtual int executeGet(const QnRequestParamList &params
        , QByteArray &result, QByteArray &contentType
        , int port);

    virtual int executePost(const QnRequestParamList &params
        , const QByteArray &body, const QByteArray &srcBodyContentType
        , QByteArray &result, QByteArray &resultContentType
        , int port);


private:
    const QString m_path;
};

StatisticsActionHandler::StatisticsActionHandler(const QString &path)
    : m_path(path)
{}

StatisticsActionHandler::~StatisticsActionHandler()
{}

const QString &StatisticsActionHandler::path() const
{
    return m_path;
}

int StatisticsActionHandler::executeGet(const QnRequestParamList &params
    , QByteArray &result, QByteArray &contentType, int port)
{
    return nx_http::StatusCode::internalServerError;
}

int StatisticsActionHandler::executePost(const QnRequestParamList &params
    , const QByteArray &body, const QByteArray &srcBodyContentType
    , QByteArray &result, QByteArray &resultContentType
    , int port)
{
    return nx_http::StatusCode::internalServerError;
}

//

class SettingsActionHandler : public StatisticsActionHandler
{
    typedef StatisticsActionHandler base_type;

public:
    SettingsActionHandler(const QString &baseUrl);

    virtual ~SettingsActionHandler();

    int executeGet(const QnRequestParamList &params
        , QByteArray &result, QByteArray &contentType
        , int port) override;

private:
    typedef QnMultiserverRequestContext<QnEmptyRequestData> Context;

    nx_http::StatusCode::Value loadSettingsLocally(QnStatisticsSettings &outputSettings);

    nx_http::StatusCode::Value loadSettingsRemotely(QnStatisticsSettings &outputSettings
        , const QnMediaServerResourcePtr &server
        , Context *context);

private:
};

SettingsActionHandler::SettingsActionHandler(const QString &baseUrl)
    : base_type(baseUrl)
{}

SettingsActionHandler::~SettingsActionHandler()
{}

int SettingsActionHandler::executeGet(const QnRequestParamList &params
    , QByteArray &result, QByteArray &contentType
    , int port)
{
    const QnEmptyRequestData request =
        QnMultiserverRequestData::fromParams<QnEmptyRequestData>(params);

    Context context(request, port);

    QnStatisticsSettings settings;
    nx_http::StatusCode::Value resultCode = nx_http::StatusCode::noContent;

    resultCode = loadSettingsLocally(settings);
    if (resultCode == nx_http::StatusCode::ok)
    {
        QnFusionRestHandlerDetail::serialize(settings, result, contentType
            , Qn::JsonFormat, request.extraFormatting);
        return nx_http::StatusCode::ok;
    }

    if (request.isLocal)
        return resultCode;

    const auto moduleGuid = qnCommon->moduleGUID();
    for (const auto server: qnResPool->getAllServers(Qn::Online))
    {
        if (server->getId() == moduleGuid)
            continue;

        resultCode = loadSettingsRemotely(settings, server, &context);
        if (resultCode == nx_http::StatusCode::ok)
            break;
    }

    if (resultCode != nx_http::StatusCode::ok)
        return resultCode;

    QnFusionRestHandlerDetail::serialize(settings, result, contentType
        , Qn::JsonFormat, request.extraFormatting);
    return nx_http::StatusCode::ok;
}

nx_http::StatusCode::Value SettingsActionHandler::loadSettingsLocally(QnStatisticsSettings &outputSettings)
{
    const auto moduleGuid = qnCommon->moduleGUID();
    const auto server = qnResPool->getResourceById<QnMediaServerResource>(moduleGuid);
    if (!server || !hasInternetConnection(server))
        return nx_http::StatusCode::noContent;

    static const auto settingsUrl = []()
    {
        static const auto kSettingsUrl =
            ec2::Ec2StaticticsReporter::DEFAULT_SERVER_API + lit("/config/client_stats.json");

        // TODO: #ynikitenkov fix to use qnGlobalSettings in 2.6
        const auto admin = qnResPool->getAdministrator();
        const auto localSettingsUrl = (admin ? admin->getProperty(
            QnMultiserverStatisticsRestHandler::kSettingsUrlParam) : QString());
        return QUrl::fromUserInput(localSettingsUrl.trimmed().isEmpty()
            ? kSettingsUrl : localSettingsUrl);
    }();

    if (SettingsCache::copyTo(outputSettings))
        return nx_http::StatusCode::ok;

    SettingsCache::clear();

    nx_http::BufferType buffer;
    int statusCode = nx_http::StatusCode::noContent;
    if (nx_http::downloadFileSync(settingsUrl, &statusCode, &buffer) != SystemError::noError)
        return nx_http::StatusCode::noContent;

    if (!QJson::deserialize(buffer, &outputSettings))
        return nx_http::StatusCode::internalServerError;

    SettingsCache::update(outputSettings);
    return nx_http::StatusCode::ok;
}

nx_http::StatusCode::Value SettingsActionHandler::loadSettingsRemotely(
    QnStatisticsSettings &outputSettings
    , const QnMediaServerResourcePtr &server
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

    const QUrl apiUrl = getServerApiUrl(path(), server, *context);
    runMultiserverDownloadRequest(apiUrl, server, completionFunc, context);
    context->waitForDone();
    return result;
}

//

class SendStatisticsActionHandler : public StatisticsActionHandler
{
    typedef StatisticsActionHandler base_type;

public:
    SendStatisticsActionHandler(const QString &basePath);

    virtual ~SendStatisticsActionHandler();

    int executePost(const QnRequestParamList &params
        , const QByteArray &body, const QByteArray &srcBodyContentType
        , QByteArray &result, QByteArray &resultContentType
        , int port);

private:
    typedef QnMultiserverRequestContext<QnSendStatisticsRequestData> Context;

    nx_http::StatusCode::Value sendStatisticsLocally(const QByteArray &metricsList
        , const QString &statisticsServerUrl);

    nx_http::StatusCode::Value sendStatisticsRemotely(const QByteArray &metricsList
        , const QnMediaServerResourcePtr &server
        , Context *context);
};

SendStatisticsActionHandler::SendStatisticsActionHandler(const QString &basePath)
    : base_type(basePath)
{}

SendStatisticsActionHandler::~SendStatisticsActionHandler()
{}

int SendStatisticsActionHandler::executePost(const QnRequestParamList& params
    , const QByteArray &body, const QByteArray &srcBodyContentType
    , QByteArray &/* result */, QByteArray &/*resultContentType*/
    , int port)
{
    QnSendStatisticsRequestData request =
        QnMultiserverRequestData::fromParams<QnSendStatisticsRequestData>(params);

    // TODO: #ynikitenkov add support of specified in parameters format, not only json!
    const bool correctJson = QJson::deserialize<QnMetricHashesList>(
        body, &request.metricsList);

    Q_ASSERT_X(correctJson, Q_FUNC_INFO, "Incorect json with mertics received!");
    if (!correctJson)
        return nx_http::StatusCode::invalidParameter;

    Context context(request, port);

    nx_http::StatusCode::Value resultCode =
        sendStatisticsLocally(body, request.statisticsServerUrl);

    if (request.isLocal)
        return resultCode;

    const auto moduleGuid = qnCommon->moduleGUID();
    for (const auto server: qnResPool->getAllServers(Qn::Online))
    {
        if (server->getId() == moduleGuid)
            continue;

        resultCode = sendStatisticsRemotely(body, server, &context);
        if (resultCode == nx_http::StatusCode::ok)
            break;
    }
    return resultCode;
}

nx_http::StatusCode::Value SendStatisticsActionHandler::sendStatisticsLocally(
    const QByteArray &metricsList
    , const QString &statisticsServerUrl)
{
    const auto moduleGuid = qnCommon->moduleGUID();
    const auto server = qnResPool->getResourceById<QnMediaServerResource>(moduleGuid);
    if (!server || !hasInternetConnection(server))
        return nx_http::StatusCode::notAcceptable;

    auto httpCode = nx_http::StatusCode::notAcceptable;
    const auto error = nx_http::uploadDataSync(statisticsServerUrl, metricsList
        , kJsonContentType, kSendStatUser, kSendStatPass, &httpCode);

    return (error == SystemError::noError
        ? httpCode : nx_http::StatusCode::internalServerError);
}

nx_http::StatusCode::Value SendStatisticsActionHandler::sendStatisticsRemotely(
    const QByteArray &metricsList
    , const QnMediaServerResourcePtr &server
    , Context *context)
{
    auto result = nx_http::StatusCode::notAcceptable;

    if (!hasInternetConnection(server))
        return result;

    const auto completionFunc = [&result, context]
        (SystemError::ErrorCode errorCode, int statusCode)
    {
        const auto httpCode = static_cast<nx_http::StatusCode::Value>(statusCode);
        const auto updateOutputDataCallback =[&result, errorCode, httpCode, context]()
        {
            result = (errorCode == SystemError::noError
                ? httpCode : nx_http::StatusCode::internalServerError);
            context->requestProcessed();
        };

        context->executeGuarded(updateOutputDataCallback);
    };

    const QUrl apiUrl = getServerApiUrl(path(), server, *context);
    runMultiserverUploadRequest(apiUrl, metricsList, kJsonContentType
        , kSendStatUser, kSendStatPass, server, completionFunc, context);
    context->waitForDone();
    return result;
}
//

QnMultiserverStatisticsRestHandler::QnMultiserverStatisticsRestHandler(const QString &basePath)
    : QnFusionRestHandler()
{
    const auto settingsPath = makeFullPath(basePath, lit("settings"));
    m_handlers.insert(settingsPath, createHandler<SettingsActionHandler>(settingsPath));

    const auto sendPath = makeFullPath(basePath, lit("send"));
    m_handlers.insert(sendPath, createHandler<SendStatisticsActionHandler>(sendPath));
}

QnMultiserverStatisticsRestHandler::~QnMultiserverStatisticsRestHandler()
{}

int QnMultiserverStatisticsRestHandler::executeGet(const QString& path
    , const QnRequestParamList& params
    , QByteArray& result
    , QByteArray& contentType
    , const QnRestConnectionProcessor *processor)
{
    const auto executeGetRequest = [params, processor, &result, &contentType]
        (const StatisticsActionHandlerPtr &handler)
    {
        return handler->executeGet(params, result, contentType
            , processor->owner()->getPort());
    };

    return processRequest(path, executeGetRequest);
}

int QnMultiserverStatisticsRestHandler::executePost(const QString& path
    , const QnRequestParamList& params
    , const QByteArray& body
    , const QByteArray& srcBodyContentType
    , QByteArray& result
    , QByteArray& resultContentType
    , const QnRestConnectionProcessor *processor)
{
    const auto executePostRequest = [params, body, srcBodyContentType
        , processor, &result, &resultContentType]
        (const StatisticsActionHandlerPtr &handler)
    {
        return handler->executePost(params, body, srcBodyContentType
            , result, resultContentType, processor->owner()->getPort());
    };

    return processRequest(path, executePostRequest);
}

int QnMultiserverStatisticsRestHandler::processRequest(const QString &fullPath
    , const RunHandlerFunc &runHandler)
{
    if (!runHandler)
        return nx_http::StatusCode::badRequest;

    const auto handlerIt = m_handlers.find(fullPath);
    if (handlerIt == m_handlers.end())
        return nx_http::StatusCode::notFound;

    const auto handler = handlerIt.value();
    return runHandler(handler);
}

