#include "multiserver_statistics_rest_handler.h"

#include <QtGlobal>

#include <common/common_module.h>
#include <common/common_module_aware.h>

#include <utils/math/math.h>
#include <network/tcp_listener.h>
#include <nx/fusion/model_functions.h>
#include <nx/network/deprecated/asynchttpclient.h>
#include <api/helpers/empty_request_data.h>
#include <api/helpers/send_statistics_request_data.h>
#include <rest/helpers/request_context.h>
#include <rest/helpers/request_helpers.h>
#include <rest/server/rest_connection_processor.h>
#include <core/resource/media_server_resource.h>
#include <core/resource_management/resource_pool.h>
#include <ec2_statictics_reporter.h>
#include <api/global_settings.h>

#include <statistics/statistics_settings.h>

namespace {

const auto kSendStatUser = lit("nx");
const auto kSendStatPass = lit("f087996adb40eaed989b73e2d5a37c951f559956c44f6f8cdfb6f127ca4136cd");
const nx_http::StringType kJsonContentType = Qn::serializationFormatToHttpContentType(Qn::JsonFormat);

QString makeFullPath(const QString& basePath, const QString& postfix)
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
QUrl getServerApiUrl(const QString& path, const QnMediaServerResourcePtr& server,
    const ContextType& context)
{
    QUrl result(server->getApiUrl());
    result.setPath(path);

    auto modifiedRequest = context.request();
    modifiedRequest.makeLocal();
    result.setQuery(modifiedRequest.toUrlQuery());

    return result;
}

bool hasInternetConnection(const QnMediaServerResourcePtr& server)
{
    const auto flags = server->getServerFlags();
    return flags.testFlag(Qn::SF_HasPublicIP);
}

bool isCorrectMetricsListJson(const QByteArray& body)
{
    QnMetricHashesList dummy;
    return QJson::deserialize(body, &dummy);
};

bool isSuccessHttpCode(nx_http::StatusCode::Value code)
{
    return qBetween<int>(nx_http::StatusCode::ok, code,
        nx_http::StatusCode::lastSuccessCode + 1);
}

class SettingsCache
{
public:
    static void update(const QnStatisticsSettings& settings);
    static void clear();
    static bool copyTo(QnStatisticsSettings& output);

private:
    SettingsCache();
    static SettingsCache &instance();

private:
    typedef QScopedPointer<QnStatisticsSettings> SettingsPtr;

    QnMutex m_mutex;
    QElapsedTimer m_timer;
    SettingsPtr m_settings;
};

SettingsCache::SettingsCache():
    m_mutex(),
    m_timer(),
    m_settings()
{
    m_timer.start();
}

SettingsCache& SettingsCache::instance()
{
    static SettingsCache inst;
    return inst;
}

void SettingsCache::update(const QnStatisticsSettings& settings)
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

bool SettingsCache::copyTo(QnStatisticsSettings& output)
{
    const QnMutexLocker guard(&instance().m_mutex);

    static const int kMinSettingsUpdateTimeMs = 4 * 60 * 60 * 1000; // every 4 hours

    auto &settings = instance().m_settings;
    auto &timer = instance().m_timer;
    if (!settings || timer.hasExpired(kMinSettingsUpdateTimeMs))
        return false;

    output = *settings;
    return true;
}

} // namespace

class AbstractStatisticsActionHandler
{
public:
    AbstractStatisticsActionHandler(const QString &path): m_path(path) {}

    virtual ~AbstractStatisticsActionHandler() {};

    const QString& path() const { return m_path; }

    virtual int executeGet(
        QnCommonModule* /*commonModule*/,
        const QnRequestParamList& /*params*/,
        QByteArray& /*result*/,
        QByteArray& /*resultContentType*/)
    {
        return nx_http::StatusCode::internalServerError;
    }

    virtual int executePost(
        QnCommonModule* /*commonModule*/,
        const QnRequestParamList& /*params*/,
        const QByteArray& /*body*/,
        const QByteArray& /*srcBodyContentType*/,
        QByteArray& /*result*/,
        QByteArray& /*resultContentType*/)
    {
        return nx_http::StatusCode::internalServerError;
    }

private:
    const QString m_path;
};

class SettingsActionHandler: public AbstractStatisticsActionHandler
{
    using base_type = AbstractStatisticsActionHandler;

public:
    SettingsActionHandler(const QString& path);
    virtual ~SettingsActionHandler();

    virtual int executeGet(
        QnCommonModule* commonModule,
        const QnRequestParamList& params,
        QByteArray& result,
        QByteArray& resultContentType) override;

private:
    typedef QnMultiserverRequestContext<QnEmptyRequestData> Context;
    nx_http::StatusCode::Value loadSettingsLocally(QnCommonModule* commonModule,
        QnStatisticsSettings& outputSettings);
};

SettingsActionHandler::SettingsActionHandler(const QString& path):
    base_type(path)
{
}

SettingsActionHandler::~SettingsActionHandler()
{
}

int SettingsActionHandler::executeGet(
    QnCommonModule* commonModule,
    const QnRequestParamList& params,
    QByteArray& result,
    QByteArray& resultContentType)
{
    const QnEmptyRequestData request = QnMultiserverRequestData::fromParams<QnEmptyRequestData>
        (commonModule->resourcePool(), params);

    QnStatisticsSettings settings;
    nx_http::StatusCode::Value resultCode = nx_http::StatusCode::notFound;

    resultCode = loadSettingsLocally(commonModule, settings);
    if (isSuccessHttpCode(resultCode))
    {
        QnFusionRestHandlerDetail::serialize(settings, result, resultContentType, Qn::JsonFormat,
            request.extraFormatting);
        return nx_http::StatusCode::ok;
    }

    return resultCode;
}

nx_http::StatusCode::Value SettingsActionHandler::loadSettingsLocally(QnCommonModule* commonModule,
    QnStatisticsSettings& outputSettings)
{
    const auto moduleGuid = commonModule->moduleGUID();
    const auto server = commonModule->resourcePool()->getResourceById<QnMediaServerResource>
        (moduleGuid);

    if (!server || !hasInternetConnection(server))
        return nx_http::StatusCode::serviceUnavailable;

    const auto settingsUrl = [commonModule]()
    {
        static const auto kSettingsUrl =
            ec2::Ec2StaticticsReporter::DEFAULT_SERVER_API + lit("/config/client_stats_v2.json");

        const auto localSettingsUrl = commonModule->globalSettings()->clientStatisticsSettingsUrl();
        return nx::utils::Url::fromUserInput(localSettingsUrl.trimmed().isEmpty()
            ? kSettingsUrl
            : localSettingsUrl);
    }();

    if (SettingsCache::copyTo(outputSettings))
        return nx_http::StatusCode::ok;

    SettingsCache::clear();

    nx_http::BufferType buffer;
    int statusCode = nx_http::StatusCode::notFound;
    if (nx_http::downloadFileSync(settingsUrl, &statusCode, &buffer) != SystemError::noError)
        return nx_http::StatusCode::notFound;

    if (!QJson::deserialize(buffer, &outputSettings))
        return nx_http::StatusCode::internalServerError;

    SettingsCache::update(outputSettings);
    return nx_http::StatusCode::ok;
}

class SendStatisticsActionHandler: public AbstractStatisticsActionHandler
{
    using base_type = AbstractStatisticsActionHandler;

public:
    SendStatisticsActionHandler(const QString& path);

    virtual ~SendStatisticsActionHandler();

    int executePost(
        QnCommonModule* commonModule,
        const QnRequestParamList& params,
        const QByteArray& body,
        const QByteArray& srcBodyContentType,
        QByteArray& result,
        QByteArray& resultContentType);

private:
    typedef QnMultiserverRequestContext<QnSendStatisticsRequestData> Context;

    nx_http::StatusCode::Value sendStatisticsLocally(
        QnCommonModule* commonModule,
        const QByteArray& metricsList,
        const QString& statisticsServerUrl);
};

SendStatisticsActionHandler::SendStatisticsActionHandler(const QString& path):
    base_type(path)
{
}

SendStatisticsActionHandler::~SendStatisticsActionHandler()
{
}

int SendStatisticsActionHandler::executePost(
    QnCommonModule* commonModule,
    const QnRequestParamList& params,
    const QByteArray& body,
    const QByteArray& srcBodyContentType,
    QByteArray& /*result*/,
    QByteArray& /*resultContentType*/)
{
    QnSendStatisticsRequestData request =
        QnMultiserverRequestData::fromParams<QnSendStatisticsRequestData>
        (commonModule->resourcePool(), params);

    // TODO: #ynikitenkov: Add support for format specified in parameters, not only json.
    const bool correctJson = QJson::deserialize<QnMetricHashesList>(
        body, &request.metricsList);

    NX_ASSERT(correctJson, Q_FUNC_INFO, "Incorrect json with metrics received!");
    if (!correctJson)
        return nx_http::StatusCode::invalidParameter;

    return sendStatisticsLocally(commonModule, body, request.statisticsServerUrl);
}

nx_http::StatusCode::Value SendStatisticsActionHandler::sendStatisticsLocally(
    QnCommonModule* commonModule,
    const QByteArray& metricsList,
    const QString& statisticsServerUrl)
{
    const auto moduleGuid = commonModule->moduleGUID();
    const auto server = commonModule->resourcePool()->getResourceById
        <QnMediaServerResource>(moduleGuid);

    if (!server || !hasInternetConnection(server))
        return nx_http::StatusCode::notAcceptable;

    auto httpCode = nx_http::StatusCode::notAcceptable;
    const auto error = nx_http::uploadDataSync(
        statisticsServerUrl,
        metricsList,
        kJsonContentType,
        kSendStatUser,
        kSendStatPass,
        nx_http::AuthType::authBasicAndDigest,
        &httpCode);

    return error == SystemError::noError
        ? httpCode
        : nx_http::StatusCode::internalServerError;
}

QnMultiserverStatisticsRestHandler::QnMultiserverStatisticsRestHandler(const QString& path):
    QnFusionRestHandler()
{
    const auto settingsPath = makeFullPath(path, lit("settings"));
    m_handlers.insert(settingsPath, createHandler<SettingsActionHandler>(settingsPath));

    const auto sendPath = makeFullPath(path, lit("send"));
    m_handlers.insert(sendPath, createHandler<SendStatisticsActionHandler>(sendPath));
}

QnMultiserverStatisticsRestHandler::~QnMultiserverStatisticsRestHandler()
{
}

int QnMultiserverStatisticsRestHandler::executeGet(
    const QString& path,
    const QnRequestParamList& params,
    QByteArray& result,
    QByteArray& resultContentType,
    const QnRestConnectionProcessor* processor)
{
    const auto executeGetRequest =
        [params, &result, &resultContentType, commonModule = processor->commonModule()]
        (const StatisticsActionHandlerPtr& handler)
        {
            return handler->executeGet(
                commonModule,
                params,
                result,
                resultContentType);
        };

    return processRequest(path, executeGetRequest);
}

int QnMultiserverStatisticsRestHandler::executePost(
    const QString& path,
    const QnRequestParamList& params,
    const QByteArray& body,
    const QByteArray& srcBodyContentType,
    QByteArray& result,
    QByteArray& resultContentType,
    const QnRestConnectionProcessor* processor)
{
    const auto executePostRequest =
        [params, body, srcBodyContentType, &result, &resultContentType, commonModule = processor->commonModule()]
        (const StatisticsActionHandlerPtr &handler)
        {
            return handler->executePost(
                commonModule,
                params,
                body,
                srcBodyContentType,
                result,
                resultContentType);
        };

    return processRequest(path, executePostRequest);
}

int QnMultiserverStatisticsRestHandler::processRequest(const QString& fullPath,
    const RunHandlerFunc& runHandler)
{
    if (!runHandler)
        return nx_http::StatusCode::badRequest;

    const auto handlerIt = m_handlers.find(fullPath);
    if (handlerIt == m_handlers.end())
        return nx_http::StatusCode::notFound;

    const auto handler = handlerIt.value();
    return runHandler(handler);
}

