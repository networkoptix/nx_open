// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "server_rest_connection.h"

#include <atomic>

#include <QtNetwork/QAuthenticator>

#include <api/helpers/chunks_request_data.h>
#include <api/helpers/empty_request_data.h>
#include <api/helpers/event_log_multiserver_request_data.h>
#include <api/helpers/event_log_request_data.h>
#include <api/helpers/send_statistics_request_data.h>
#include <api/helpers/thumbnail_request_data.h>
#include <api/model/cloud_credentials_data.h>
#include <api/model/update_information_reply.h>
#include <common/static_common_module.h>
#include <core/resource/camera_resource.h>
#include <core/resource/media_server_resource.h>
#include <core/resource/user_resource.h>
#include <core/resource_management/resource_pool.h>
#include <core/resource_management/resource_properties.h>
#include <network/router.h>
#include <nx/api/mediaserver/image_request.h>
#include <nx/branding.h>
#include <nx/build_info.h>
#include <nx/fusion/model_functions.h>
#include <nx/fusion/serialization/compressed_time_functions.h>
#include <nx/metrics/application_metrics_storage.h>
#include <nx/network/http/custom_headers.h>
#include <nx/network/http/http_types.h>
#include <nx/network/ssl/helpers.h>
#include <nx/network/url/url_builder.h>
#include <nx/reflect/json.h>
#include <nx/reflect/string_conversion.h>
#include <nx/reflect/urlencoded/serializer.h>
#include <nx/utils/buffer.h>
#include <nx/utils/guarded_callback.h>
#include <nx/utils/log/log.h>
#include <nx/utils/random.h>
#include <nx/utils/serialization/qjson.h>
#include <nx/utils/serialization/qt_containers_reflect_json.h>
#include <nx/vms/api/analytics/device_agent_active_setting_changed_request.h>
#include <nx/vms/api/analytics/device_agent_settings_request.h>
#include <nx/vms/api/data/device_actions.h>
#include <nx/vms/api/data/ldap.h>
#include <nx/vms/api/data/peer_data.h>
#include <nx/vms/api/data/storage_encryption_data.h>
#include <nx/vms/api/data/system_information.h>
#include <nx/vms/api/rules/event_log.h>
#include <nx/vms/common/api/helpers/parser_helper.h>
#include <nx/vms/common/application_context.h>
#include <nx/vms/common/network/abstract_certificate_verifier.h>
#include <nx/vms/common/resource/analytics_engine_resource.h>
#include <nx/vms/common/resource/analytics_plugin_resource.h>
#include <nx/vms/common/system_context.h>
#include <nx/vms/event/rule.h>
#include <nx/vms/event/rule_manager.h>
#include <nx_ec/abstract_ec_connection.h>
#include <nx_ec/data/api_conversion_functions.h>
#include <utils/common/delayed.h>

using namespace nx;

namespace {

constexpr int kMessageBodyLogSize = 50;
constexpr auto kJsonRpcPath = "/jsonrpc";

// Type helper for parsing function overloading.
template <typename T>
struct Type {};

// Response deserialization for RestResultWithData objects.
template<typename T>
rest::RestResultWithData<T> parseMessageBody(
    Type<rest::RestResultWithData<T>>,
    Qn::SerializationFormat format,
    const QByteArray& msgBody,
    nx::network::http::StatusCode::Value statusCode,
    bool* success)
{
    using Result = rest::RestResultWithData<T>;
    switch (format)
    {
        case Qn::SerializationFormat::json:
        {
            auto restResult =
                QJson::deserialized(msgBody, nx::network::rest::JsonResult(), success);
            return Result(restResult, restResult.deserialized<T>());
        }
        case Qn::SerializationFormat::ubjson:
        {
            auto restResult =
                QnUbjson::deserialized(msgBody, nx::network::rest::UbjsonResult(), success);
            return Result(restResult, restResult.deserialized<T>());
        }
        default:
            *success = false;

            NX_DEBUG(typeid(rest::ServerConnection),
                "Unsupported format '%1', status code: %2, message body: %3 ...",
                nx::reflect::enumeration::toString(format),
                statusCode,
                msgBody.left(kMessageBodyLogSize));

            break;
    }
    return Result();
}

// Response deserialization for plain object types.
template<typename T>
T parseMessageBody(
    Type<T>,
    Qn::SerializationFormat format,
    const QByteArray& msgBody,
    nx::network::http::StatusCode::Value statusCode,
    bool* success)
{
    if (statusCode != nx::network::http::StatusCode::ok)
    {
        NX_DEBUG(typeid(rest::ServerConnection), "Unexpected HTTP status code: %1", statusCode);
        *success = false;
        return T();
    }

    switch (format)
    {
        case Qn::SerializationFormat::json:
            return QJson::deserialized(msgBody, T(), success);
        case Qn::SerializationFormat::ubjson:
            return QnUbjson::deserialized(msgBody, T(), success);
        default:
            *success = false;

            NX_DEBUG(typeid(rest::ServerConnection),
                "Unsupported format '%1', status code: %2, message body: %3 ...",
                nx::reflect::enumeration::toString(format),
                statusCode,
                msgBody.left(kMessageBodyLogSize));

            break;
    }
    return T();
}

// Special case of nx::network::rest::Result deserialization.
// Response body is empty on REST API success.
template<>
nx::network::rest::Result parseMessageBody(
    Type<nx::network::rest::Result>,
    Qn::SerializationFormat format,
    const QByteArray& messageBody,
    nx::network::http::StatusCode::Value httpStatusCode,
    bool* success)
{
    auto result = nx::vms::common::api::parseRestResult(httpStatusCode, format, messageBody);
    *success = (result.error == nx::network::rest::Result::NoError);

    return result;
}

// Response deserialization for ErrorOrData objects
template<typename T>
rest::ErrorOrData<T> parseMessageBody(
    Type<rest::ErrorOrData<T>>,
    Qn::SerializationFormat format,
    const QByteArray& messageBody,
    nx::network::http::StatusCode::Value httpStatusCode,
    bool* success)
{
    if (format != Qn::SerializationFormat::json)
    {
        NX_DEBUG(typeid(rest::ServerConnection),
            "Unsupported format '%1', status code: %2, message body: %3 ...",
            nx::reflect::enumeration::toString(format),
            httpStatusCode,
            messageBody.left(kMessageBodyLogSize));
    }

    if (httpStatusCode == nx::network::http::StatusCode::ok)
    {
        T data;
        if constexpr (std::is_same_v<T, rest::Empty>)
        {
            *success = true;
            return data;
        }
        else if constexpr (std::is_same_v<T, QByteArray>)
        {
            *success = true;
            return messageBody;
        }
        else
        {
            if ((*success = nx::reflect::json::deserialize(messageBody.data(), &data)))
                return data;

            NX_ASSERT(false, "Data cannot be deserialized:\n %1",
                messageBody.left(kMessageBodyLogSize));
            return nx::network::rest::Result::notImplemented();
        }
    }

    return parseMessageBody(
        Type<nx::network::rest::Result>{}, format, messageBody, httpStatusCode, success);
}

template <typename T>
T parseMessageBody(
    Qn::SerializationFormat format,
    const QByteArray& messageBody,
    nx::network::http::StatusCode::Value httpStatusCode,
    bool* success)
{
    NX_CRITICAL(success);
    return parseMessageBody(Type<T>{}, format, messageBody, httpStatusCode, success);
}

// Invokes callback in appropriate thread
void invoke(network::http::ClientPool::ContextPtr context,
    std::function<void ()> callback,
    bool success,
    const QString &serverId
)
{
    NX_ASSERT(context);
    if (!context)
        return;

    nx::utils::log::Tag tag(QString("%1 [%2]").arg(
        nx::toString(typeid(rest::ServerConnection)), serverId));

    /*
     * TODO: It can be moved to ClientPool::context
     * `targetThread` is also stored there.
     * `serverId` is still missing
     */
    auto elapsedMs = context->getTimeElapsed().count();
    if (success)
        NX_VERBOSE(tag, "<%1>: Reply success for %2ms", context->handle, elapsedMs);
    else
        NX_VERBOSE(tag, "<%1>: Reply failed for %2ms", context->handle, elapsedMs);

    if (auto thread = context->targetThread())
        executeLaterInThread(std::move(callback), thread);
    else
        callback();
}

void proxyRequestUsingServer(
    nx::network::http::ClientPool::Request& request,
    const QnUuid& proxyServerId)
{
    nx::network::http::HttpHeader header(Qn::SERVER_GUID_HEADER_NAME, proxyServerId.toByteArray());
    nx::network::http::insertOrReplaceHeader(&request.headers, header);
}

template<typename T>
rest::ServerConnection::Result<nx::network::rest::JsonResult>::type extractJsonResult(
    typename rest::ServerConnection::Result<T>::type callback)
{
    return
        [callback = std::move(callback)](bool success, rest::Handle requestId,
            const nx::network::rest::JsonResult& result)
        {
            callback(success, requestId, result.deserialized<T>());
        };
}

static bool isSessionExpiredError(int code)
{
    return code == nx::network::rest::Result::SessionExpired
        || code == nx::network::rest::Result::SessionRequired;
}

static bool isSessionExpiredError(const nx::vms::api::JsonRpcResponse& response)
{
    if (!response.error)
        return false;
    if (!response.error->data)
        return false;

    nx::network::rest::Result result;
    if (!QJson::deserialize(*response.error->data, &result))
        return false;

    return isSessionExpiredError(result.error);
}

// Returns '*' on null id.
QString formatRestId(QnUuid id)
{
    return id.isNull() ? QString("*") : id.toSimpleString();
}

std::string prepareUserAgent()
{
    static const QMap<nx::vms::api::PeerType, std::string_view> kPeerTypeToUserAgent = {
        {nx::vms::api::PeerType::server, "VMS Server"},
        {nx::vms::api::PeerType::desktopClient, "Desktop Client"},
        {nx::vms::api::PeerType::videowallClient, "VideoWall Client"},
        {nx::vms::api::PeerType::oldMobileClient, "Old Mobile Client"},
        {nx::vms::api::PeerType::mobileClient, "Mobile Client"},
        {nx::vms::api::PeerType::cloudServer, "Cloud Server"},
        {nx::vms::api::PeerType::oldServer, "Old VMS Server"},
        {nx::vms::api::PeerType::notDefined, "Not Defined"}};


    return NX_FMT("%1 %2 %3",
        nx::branding::vmsName(),
        kPeerTypeToUserAgent.value(nx::vms::common::appContext()->localPeerType(), "Unknown Peer"),
        nx::build_info::vmsVersion()).toStdString();
}

} // namespace

// --------------------------- public methods -------------------------------------------

namespace rest {

struct ServerConnection::Private
{
    Private(
        nx::vms::common::SystemContext* systemContext,
        const QnUuid& serverId,
        const nx::utils::log::Tag& logTag)
        :
        systemContext(systemContext),
        serverId(serverId),
        logTag(logTag),
        httpClientPool(new nx::network::http::ClientPool()),
        reissuedToken(new ReissuedToken())
    {
    }

    const nx::vms::common::SystemContext* systemContext = nullptr;
    const QnUuid serverId;
    const nx::utils::log::Tag logTag;
    const QScopedPointer<nx::network::http::ClientPool> httpClientPool;

    // While most fields of this struct never change during struct's lifetime, some data can be
    // rarely updated. Therefore the following non-const fields should be protected by mutex.
    nx::Mutex mutex;

    struct DirectConnect
    {
        QnUuid sessionId;
        nx::vms::common::AbstractCertificateVerifier* certificateVerifier = nullptr;
        nx::network::SocketAddress address;
        nx::network::http::Credentials credentials;
    };
    std::optional<DirectConnect> directConnect;

    using ResendRequestFunc = std::function<void(std::optional<nx::network::http::AuthToken>)>;
    QList<ResendRequestFunc> storedRequests;
    std::map<Handle, Handle> substitutions;

    // Authorization token helper. The data is accessed only in the main application thread.
    class ReissuedToken
    {
    public:
        bool hasValue() const
        {
            return m_isSet;
        }
        std::optional<nx::network::http::AuthToken> value() const
        {
            NX_ASSERT(m_isSet);
            return m_value;
        }
        void setValue(const std::optional<nx::network::http::AuthToken>& value)
        {
            NX_ASSERT(!m_isSet);
            m_isSet = true;
            m_value = value;
        }

    private:
        bool m_isSet = false;
        std::optional<nx::network::http::AuthToken> m_value;
    };
    using ReissuedTokenPtr = std::shared_ptr<ReissuedToken>;

    // Pointer to the helper. Could be accessed in any thread and should be protected by mutex.
    ReissuedTokenPtr reissuedToken;
};

ServerConnection::ServerConnection(
    nx::vms::common::SystemContext* systemContext,
    const QnUuid& serverId)
    :
    QObject(),
    d(new Private(
        systemContext,
        serverId,
        nx::utils::log::Tag(
            QStringLiteral("%1 [%2]").arg(nx::toString(this), serverId.toString()))))
{
    // TODO: #sivanov Raw pointer is unsafe here as ServerConnection instance may be not deleted
    // after it's owning server (and context) are destroyed. Need to change
    // QnMediaServerResource::restConnection() method to return weak pointer instead.
}

ServerConnection::ServerConnection(
    const QnUuid& serverId,
    const QnUuid& sessionId,
    AbstractCertificateVerifier* certificateVerifier,
    nx::network::SocketAddress address,
    nx::network::http::Credentials credentials)
    :
    ServerConnection(/*systemContext*/ nullptr, serverId)
{
    d->directConnect = Private::DirectConnect();
    d->directConnect->sessionId = sessionId;
    d->directConnect->certificateVerifier = certificateVerifier;
    d->directConnect->address = std::move(address);
    d->directConnect->credentials = std::move(credentials);
}

void ServerConnection::updateSessionId(const QnUuid& sessionId)
{
    NX_MUTEX_LOCKER lock(&d->mutex);

    if (NX_ASSERT(d->directConnect))
        d->directConnect->sessionId = sessionId;
}

ServerConnection::~ServerConnection()
{
    d->httpClientPool->stop(/*invokeCallbacks*/ true);
}

void ServerConnection::updateAddress(nx::network::SocketAddress address)
{
    NX_MUTEX_LOCKER lock(&d->mutex);

    if (NX_ASSERT(d->directConnect))
        d->directConnect->address = std::move(address);
}

void ServerConnection::updateCredentials(nx::network::http::Credentials credentials)
{
    NX_MUTEX_LOCKER lock(&d->mutex);

    // All requests must be made with session credentials, and should only be changed if the new
    // credentials are session ones.
    if (NX_ASSERT(d->directConnect) && credentials.authToken.isBearerToken())
        d->directConnect->credentials = std::move(credentials);
}

void ServerConnection::setDefaultTimeouts(nx::network::http::AsyncClient::Timeouts timeouts)
{
    d->httpClientPool->setDefaultTimeouts(timeouts);
}

Handle ServerConnection::cameraHistoryAsync(
    const QnChunksRequestData& request,
    Result<nx::vms::api::CameraHistoryDataList>::type callback,
    QThread* targetThread)
{
    return executeGet(lit("/ec2/cameraHistory"), request.toParams(), callback, targetThread);
}

Handle ServerConnection::backupPositionAsync(const QnUuid& serverId,
    const QnUuid& deviceId,
    Result<nx::vms::api::BackupPositionEx>::type callback,
    QThread* targetThread)
{
    const auto requestStr =
        NX_FMT("/rest/v1/servers/%1/backupPositions/%2").args(serverId, deviceId);
    return executeGet(requestStr, nx::network::rest::Params(), callback, targetThread);
}

Handle ServerConnection::setBackupPositionAsync(const QnUuid& serverId,
    const QnUuid& deviceId,
    const nx::vms::api::BackupPosition& backupPosition,
    Result<nx::vms::api::BackupPosition>::type callback,
    QThread* targetThread)
{
    const auto requestStr =
        NX_FMT("/rest/v1/servers/%1/backupPositions/%2").args(serverId, deviceId);
    return executePut(
        requestStr,
        nx::network::rest::Params(),
        "application/json",
        QJson::serialized(backupPosition),
        callback,
        targetThread);
}

Handle ServerConnection::setBackupPositionsAsync(const QnUuid& serverId,
    const nx::vms::api::BackupPosition& backupPosition,
    ServerConnection::Result<nx::vms::api::BackupPosition>::type callback,
    QThread* targetThread)
{
    const auto requestStr = NX_FMT("/rest/v1/servers/%1/backupPositions", serverId);
    return executePut(
        requestStr,
        nx::network::rest::Params(),
        "application/json",
        QJson::serialized(backupPosition),
        callback,
        targetThread);
}

Handle ServerConnection::getServerLocalTime(
    const QnUuid& serverId,
    Result<nx::network::rest::JsonResult>::type callback,
    QThread* targetThread)
{
    nx::network::rest::Params params{{"local", QnLexical::serialized(true)}};
    return executeGet("/api/gettime", params, callback, targetThread, serverId);
}

rest::Handle ServerConnection::cameraThumbnailAsync(const nx::api::CameraImageRequest& request,
    Result<QByteArray>::type callback,
    QThread* targetThread)
{
    if (debugFlags().testFlag(DebugFlag::disableThumbnailRequests))
        return {};

    QnThumbnailRequestData data;
    data.request = request;
    data.format = Qn::SerializationFormat::ubjson;

    return executeGet(lit("/ec2/cameraThumbnail"), data.toParams(), callback, targetThread);
}

Handle ServerConnection::createGenericEvent(
    const QString& source,
    const QString& caption,
    const QString& description,
    const nx::vms::event::EventMetaData& metadata,
    nx::vms::api::EventState toggleState,
    GetCallback callback,
    QThread* targetThread)
{
    nx::network::rest::Params params;
    params.insert("source", source);
    params.insert("caption", caption);
    params.insert("description", description);
    if (toggleState != nx::vms::api::EventState::undefined)
        params.insert("state", nx::reflect::toString(toggleState));
    params.insert("metadata", QString::fromUtf8(QJson::serialized(metadata)));
    return executePost("/api/createEvent", params, callback, targetThread);
}

Handle ServerConnection::sendStatisticsUsingServer(
    const QnUuid& proxyServerId,
    const QnSendStatisticsRequestData& statisticsData,
    PostCallback callback,
    QThread* targetThread)
{
    static const QString kPath = "/ec2/statistics/send";

    using namespace nx::network::http;
    ClientPool::Request request = prepareRequest(
        Method::post,
        prepareUrl(kPath, statisticsData.toParams()),
        header::ContentType::kJson.toString(),
        QJson::serialized(statisticsData.metricsList));
    proxyRequestUsingServer(request, proxyServerId);

    auto handle = request.isValid() ? executeRequest(request, callback, targetThread) : Handle();
    NX_VERBOSE(d->logTag, "<%1> %2", handle, request.url);
    return handle;
}

Handle ServerConnection::getModuleInformation(
    Result<RestResultWithData<nx::vms::api::ModuleInformation>>::type callback,
    QThread* targetThread)
{
    nx::network::rest::Params params;
    return executeGet("/api/moduleInformation", params, callback, targetThread);
}

Handle ServerConnection::getModuleInformationAll(
    Result<RestResultWithData<QList<nx::vms::api::ModuleInformation>>>::type callback,
    QThread* targetThread)
{
    nx::network::rest::Params params;
    params.insert("allModules", lit("true"));
    return executeGet("/api/moduleInformation", params, callback, targetThread);
}

Handle ServerConnection::getMediaServers(
    Result<nx::vms::api::MediaServerDataList>::type callback,
    QThread* targetThread)
{
    nx::network::rest::Params params;
    return executeGet("/ec2/getMediaServers", params, callback, targetThread);
}

Handle ServerConnection::getServersInfo(
    Result<ErrorOrData<nx::vms::api::ServerInformationList>>::type&& callback,
    QThread* targetThread)
{
    return executeGet("/rest/v1/servers/*/info", {}, std::move(callback), targetThread);
}

Handle ServerConnection::bindSystemToCloud(
    const QString& cloudSystemId,
    const QString& cloudAuthKey,
    const QString& cloudAccountName,
    const std::string& ownerSessionToken,
    Result<ErrorOrEmpty>::type callback,
    QThread* targetThread)
{
    nx::vms::api::CloudSystemAuth data;
    data.systemId = cloudSystemId;
    data.authKey = cloudAuthKey;
    data.owner = cloudAccountName;

    auto request = prepareRestRequest(
        nx::network::http::Method::post,
        prepareUrl("/rest/v3/system/cloud/bind", /*params*/ {}),
        nx::reflect::json::serialize(data));
    request.credentials = nx::network::http::BearerAuthToken(ownerSessionToken);

    auto handle = request.isValid()
        ? executeRequest(request, callback, targetThread)
        : Handle();

    NX_VERBOSE(d->logTag, "<%1> %2", handle, request.url);
    return handle;
}

Handle ServerConnection::unbindSystemFromCloud(
    nx::vms::common::SessionTokenHelperPtr helper,
    const QString& password,
    Result<ErrorOrEmpty>::type callback,
    QThread* targetThread)
{
    nx::vms::api::LocalSystemAuth data;
    data.password = password;

    auto request = prepareRestRequest(
        nx::network::http::Method::post,
        prepareUrl("/rest/v3/system/cloud/unbind", /*params*/ {}),
        nx::reflect::json::serialize(data));

    auto wrapper = makeSessionAwareCallback(helper, request, std::move(callback));

    auto handle = request.isValid()
        ? executeRequest(request, wrapper, targetThread)
        : Handle();

    NX_VERBOSE(d->logTag, "<%1> %2", handle, request.url);
    return handle;
}

Handle ServerConnection::dumpDatabase(
    nx::vms::common::SessionTokenHelperPtr helper,
    Result<ErrorOrData<QByteArray>>::type callback,
    QThread* targetThread)
{
    auto request = prepareRequest(
        nx::network::http::Method::get,
        prepareUrl("/rest/v2/system/database", /*params*/ {}),
        nx::network::http::header::ContentType::kBinary.value);

    auto internalCallback =
        [callback = std::move(callback)](
            bool success,
            Handle requestId,
            QByteArray body,
            const nx::network::http::HttpHeaders&)
        {
            if (success)
            {
                callback(success, requestId, body);
                return;
            }
            nx::network::rest::Result result;
            QJson::deserialize(body, &result);
            callback(success, requestId, result);
        };

    auto timeouts = nx::network::http::AsyncClient::Timeouts::defaults();
    timeouts.responseReadTimeout = std::chrono::minutes(5);
    timeouts.messageBodyReadTimeout = std::chrono::minutes(5);

    auto wrapper =
        makeSessionAwareCallback(helper, request, std::move(internalCallback), timeouts);

    auto handle = request.isValid()
        ? executeRequest(request, std::move(wrapper), targetThread, timeouts)
        : Handle();

    NX_VERBOSE(d->logTag, "<%1> %2", handle, request.url);
    return handle;
}

Handle ServerConnection::restoreDatabase(
    nx::vms::common::SessionTokenHelperPtr helper,
    const QByteArray& data,
    Result<ErrorOrEmpty>::type callback,
    QThread* targetThread)
{
    auto request = prepareRequest(
        nx::network::http::Method::post,
        prepareUrl("/rest/v2/system/database", /*params*/ {}),
        nx::network::http::header::ContentType::kBinary.value,
        data);

    auto wrapper = makeSessionAwareCallback(helper, request, std::move(callback));

    auto timeouts = nx::network::http::AsyncClient::Timeouts::defaults();
    timeouts.sendTimeout = std::chrono::minutes(5);
    timeouts.responseReadTimeout = std::chrono::minutes(5);
    auto handle = request.isValid()
        ? executeRequest(request, std::move(wrapper), targetThread, timeouts)
        : Handle();

    NX_VERBOSE(d->logTag, "<%1> %2", handle, request.url);
    return handle;
}

Handle ServerConnection::putServerLogSettings(
    nx::vms::common::SessionTokenHelperPtr helper,
    const QnUuid& serverId,
    const nx::vms::api::ServerLogSettings& settings,
    Result<ErrorOrEmpty>::type callback,
    QThread* targetThread)
{
    auto request = prepareRequest(
        nx::network::http::Method::put,
        prepareUrl(
            QString("/rest/v2/servers/%1/logSettings").arg(serverId.toString()),
            /*params*/ {}),
        Qn::serializationFormatToHttpContentType(Qn::SerializationFormat::json),
        nx::reflect::json::serialize(settings));

    auto wrapper = makeSessionAwareCallback(helper, request, std::move(callback));

    auto handle = request.isValid()
        ? executeRequest(request, std::move(wrapper), targetThread)
        : Handle();

    NX_VERBOSE(d->logTag, "<%1> %2", handle, request.url);
    return handle;
}

Handle ServerConnection::patchSystemSettings(
    nx::vms::common::SessionTokenHelperPtr helper,
    const nx::vms::api::SaveableSystemSettings& settings,
    Result<ErrorOrEmpty>::type callback,
    nx::utils::AsyncHandlerExecutor executor)
{
    auto request = prepareRequest(
        nx::network::http::Method::patch,
        prepareUrl(
            "/rest/v3/system/settings",
            /*params*/ {}),
        Qn::serializationFormatToHttpContentType(Qn::SerializationFormat::json),
        nx::reflect::json::serialize(settings));

    auto wrapper = makeSessionAwareCallback(helper, request, std::move(callback));

    auto handle = request.isValid()
        ? executeRequest(request, std::move(wrapper), executor)
        : Handle();

    NX_VERBOSE(d->logTag, "<%1> %2", handle, request.url);
    return handle;
}

Handle ServerConnection::addFileDownload(
    const QString& fileName,
    qint64 size,
    const QByteArray& md5,
    const QUrl& url,
    const QString& peerPolicy,
    GetCallback callback,
    QThread* targetThread)
{
    return executePost(
        QString("/api/downloads/%1").arg(fileName),
        nx::network::rest::Params{
            {"size", QString::number(size)},
            {"md5", QString::fromUtf8(md5)},
            {"url", url.toString()},
            {"peerPolicy", peerPolicy}},
        callback,
        targetThread);
}

Handle ServerConnection::addCamera(
    const QnUuid& targetServerId,
    const nx::vms::api::DeviceModelForSearch& device,
    nx::vms::common::SessionTokenHelperPtr helper,
    Result<ErrorOrData<nx::vms::api::DeviceModelForSearch>>::type&& callback,
    QThread* thread)
{
    auto request = prepareRequest(nx::network::http::Method::post,
        prepareUrl(QString("/rest/v3/devices"), {}),
        Qn::serializationFormatToHttpContentType(Qn::SerializationFormat::json),
        nx::reflect::json::serialize(device));

    proxyRequestUsingServer(request, targetServerId);

    auto wrapper = makeSessionAwareCallback(helper, request, std::move(callback));

    auto handle = request.isValid()
        ? executeRequest(request, std::move(wrapper), thread)
        : Handle();

    NX_VERBOSE(d->logTag, "<%1> %2", handle, request.url);
    return handle;
}

Handle ServerConnection::searchCamera(
    const QnUuid& targetServerId,
    const nx::vms::api::DeviceSearch& deviceSearchData,
    nx::vms::common::SessionTokenHelperPtr helper,
    Result<ErrorOrData<nx::vms::api::DeviceSearch>>::type&& callback,
    QThread* targetThread)
{
    auto request = prepareRequest(nx::network::http::Method::post,
        prepareUrl(QString("/rest/v3/devices/*/searches/"), {}),
        Qn::serializationFormatToHttpContentType(Qn::SerializationFormat::json),
        nx::reflect::json::serialize(deviceSearchData));

    proxyRequestUsingServer(request, targetServerId);

    auto wrapper = makeSessionAwareCallback(helper, request, std::move(callback));

    auto handle = request.isValid()
        ? executeRequest(request, std::move(wrapper), targetThread)
        : Handle();

    NX_VERBOSE(d->logTag, "<%1> %2", handle, request.url);
    return handle;
}

Handle ServerConnection::searchCameraStatus(
    const QnUuid& targetServerId,
    const QnUuid& searchId,
    nx::vms::common::SessionTokenHelperPtr helper,
    Result<ErrorOrData<nx::vms::api::DeviceSearch>>::type&& callback,
    QThread* targetThread)
{
    auto request = prepareRequest(nx::network::http::Method::get,
        prepareUrl(NX_FMT("/rest/v3/devices/*/searches/%1", searchId), {}));

    proxyRequestUsingServer(request, targetServerId);

    auto wrapper = makeSessionAwareCallback(helper, request, std::move(callback));

    auto handle = request.isValid()
        ? executeRequest(request, std::move(wrapper), targetThread)
        : Handle();

    NX_VERBOSE(d->logTag, "<%1> %2", handle, request.url);
    return handle;
}

Handle ServerConnection::searchCameraStop(
    const QnUuid& targetServerId,
    const QnUuid& searchId,
    nx::vms::common::SessionTokenHelperPtr helper,
    Result<ErrorOrEmpty>::type callback,
    QThread* targetThread)
{
    auto request = prepareRequest(nx::network::http::Method::delete_,
        prepareUrl(NX_FMT("/rest/v3/devices/*/searches/%1", searchId), {}));

    proxyRequestUsingServer(request, targetServerId);

    auto wrapper = makeSessionAwareCallback(helper, request, std::move(callback));

    auto handle = request.isValid()
        ? executeRequest(request, std::move(wrapper), targetThread)
        : Handle();

    NX_VERBOSE(d->logTag, "<%1> %2", handle, request.url);
    return handle;
}

Handle ServerConnection::executeAnalyticsAction(
    const AnalyticsAction& action,
    Result<nx::network::rest::JsonResult>::type callback,
    QThread* targetThread)
{
    return executePost(
        "/api/executeAnalyticsAction",
        QJson::serialized(action),
        std::move(callback),
        targetThread);
}

Handle ServerConnection::getRemoteArchiveSynchronizationStatus(
    Result<ErrorOrData<nx::vms::api::RemoteArchiveSynchronizationStatusList>>::type&& callback,
    QThread* targetThread)
{
    return executeGet(
        "/rest/v3/servers/this/remoteArchive/*/sync",
        nx::network::rest::Params(),
        std::move(callback),
        targetThread);
}

Handle ServerConnection::getOverlappedIds(
    const QString& nvrGroupId,
    Result<nx::vms::api::OverlappedIdResponse>::type callback,
    QThread* targetThread)
{
    return executeGet(
        "/api/overlappedIds",
        nx::network::rest::Params{{"groupId", nvrGroupId}},
        Result<nx::network::rest::JsonResult>::type(
            [callback = std::move(callback)](
                bool success, Handle requestId, const nx::network::rest::JsonResult& result)
            {
                callback(
                    success,
                    requestId,
                    result.deserialized<nx::vms::api::OverlappedIdResponse>());
            }),
        targetThread);
}

Handle ServerConnection::setOverlappedId(
    const QString& nvrGroupId,
    int overlappedId,
    Result<nx::vms::api::OverlappedIdResponse>::type callback,
    QThread* targetThread)
{
    nx::vms::api::SetOverlappedIdRequest request;
    request.groupId = nvrGroupId;
    request.overlappedId = overlappedId;

    return executePost(
        "/api/overlappedIds",
        nx::network::rest::Params(),
        Qn::serializationFormatToHttpContentType(Qn::SerializationFormat::json),
        QJson::serialized(request),
        Result<nx::network::rest::JsonResult>::type(
            [callback = std::move(callback)](
                bool success, Handle requestId, const nx::network::rest::JsonResult& result)
            {
                callback(
                    success,
                    requestId,
                    result.deserialized<nx::vms::api::OverlappedIdResponse>());
            }),
        targetThread);
}

Handle ServerConnection::executeEventAction(
    const nx::vms::api::EventActionData& action,
    Result<nx::network::rest::Result>::type callback,
    QThread* targetThread,
    std::optional<QnUuid> proxyToServer)
{
    return executePost(
        "/api/executeEventAction",
        QJson::serialized(action),
        std::move(callback),
        targetThread,
        proxyToServer);
}

Handle ServerConnection::addFileUpload(
    const QnUuid& serverId,
    const QString& fileName,
    qint64 size,
    qint64 chunkSize,
    const QByteArray& md5,
    qint64 ttl,
    bool recreateIfExists,
    AddUploadCallback callback,
    QThread* targetThread)
{
    nx::network::rest::Params params
    {
        {"size", QString::number(size)},
        {"chunkSize", QString::number(chunkSize)},
        {"md5", QString::fromUtf8(md5)},
        {"ttl", QString::number(ttl)},
        {"upload", "true"},
        {"recreate", recreateIfExists ? "true" : "false"},
    };
    const auto path = QString("/api/downloads/%1").arg(fileName);
    return executePost(
        path,
        params,
        callback,
        targetThread,
        serverId);
}

Handle ServerConnection::removeFileDownload(
    const QnUuid& serverId,
    const QString& fileName,
    bool deleteData,
    PostCallback callback,
    QThread* targetThread)
{
    return executeDelete(
        lit("/api/downloads/%1").arg(fileName),
        nx::network::rest::Params{{lit("deleteData"), QnLexical::serialized(deleteData)}},
        callback,
        targetThread,
        serverId);
}

Handle ServerConnection::fileChunkChecksums(
    const QnUuid& serverId,
    const QString& fileName,
    GetCallback callback,
    QThread* targetThread)
{
    return executeGet(
        lit("/api/downloads/%1/checksums").arg(fileName),
        nx::network::rest::Params(),
        callback,
        targetThread,
        serverId);
}

Handle ServerConnection::downloadFileChunk(
    const QnUuid& serverId,
    const QString& fileName,
    int chunkIndex,
    Result<QByteArray>::type callback,
    QThread* targetThread)
{
    return executeGet(
        nx::format("/api/downloads/%1/chunks/%2", fileName, chunkIndex),
        nx::network::rest::Params(),
        callback,
        targetThread,
        serverId);
}

Handle ServerConnection::downloadFileChunkFromInternet(
    const QnUuid& serverId,
    const QString& fileName,
    const nx::utils::Url& url,
    int chunkIndex,
    int chunkSize,
    qint64 fileSize,
    Result<QByteArray>::type callback,
    QThread* targetThread)
{
    return executeGet(
        nx::format("/api/downloads/%1/chunks/%2", fileName, chunkIndex),
        nx::network::rest::Params{
            {"url", url.toString()},
            {"chunkSize", QString::number(chunkSize)},
            {"fileSize", QString::number(fileSize)},
            {"fromInternet", "true"}},
        callback,
        targetThread,
        serverId);
}

Handle ServerConnection::uploadFileChunk(
    const QnUuid& serverId,
    const QString& fileName,
    int index,
    const QByteArray& data,
    PostCallback callback,
    QThread* targetThread)
{
    return executePut(
        lit("/api/downloads/%1/chunks/%2").arg(fileName).arg(index),
        nx::network::rest::Params(),
        "application/octet-stream",
        data,
        callback,
        targetThread,
        serverId);
}

Handle ServerConnection::downloadsStatus(
    GetCallback callback,
    QThread* targetThread)
{
    return executeGet(
        lit("/api/downloads/status"),
        nx::network::rest::Params(),
        callback,
        targetThread);
}

Handle ServerConnection::fileDownloadStatus(
    const QnUuid& serverId,
    const QString& fileName,
    GetCallback callback,
    QThread* targetThread)
{
    return executeGet(
        QString("/api/downloads/%1/status").arg(fileName),
        nx::network::rest::Params(),
        callback,
        targetThread,
        serverId);
}

Handle ServerConnection::getTimeOfServersAsync(
    Result<MultiServerTimeData>::type callback,
    QThread* targetThread)
{
    return executeGet(lit("/ec2/getTimeOfServers"), nx::network::rest::Params(), callback, targetThread);
}

Handle ServerConnection::addVirtualCamera(
    const QnUuid& serverId,
    const QString& name,
    GetCallback callback,
    QThread* targetThread)
{
    return executePost(
        "/api/virtualCamera/add",
        nx::network::rest::Params{{ "name", name }},
        callback,
        targetThread,
        serverId);
}

Handle ServerConnection::prepareVirtualCameraUploads(
    const QnNetworkResourcePtr& camera,
    const QnVirtualCameraPrepareData& data,
    GetCallback callback,
    QThread* targetThread)
{
    return executePost(
        "/api/virtualCamera/prepare",
        nx::network::rest::Params{ { "cameraId", camera->getId().toSimpleString() } },
        nx::network::http::header::ContentType::kJson.toString(),
        QJson::serialized(data),
        callback,
        targetThread,
        /*timeouts*/ {},
        camera->getParentId());
}

Handle ServerConnection::virtualCameraStatus(
    const QnNetworkResourcePtr& camera,
    GetCallback callback,
    QThread* targetThread)
{
    return executeGet(
        "/api/virtualCamera/status",
        nx::network::rest::Params{ { lit("cameraId"), camera->getId().toSimpleString() } },
        callback,
        targetThread,
        camera->getParentId());
}

Handle ServerConnection::lockVirtualCamera(
    const QnNetworkResourcePtr& camera,
    const QnUserResourcePtr& user,
    qint64 ttl,
    GetCallback callback,
    QThread* targetThread)
{
    return executePost(
        "/api/virtualCamera/lock",
        nx::network::rest::Params{
            { "cameraId", camera->getId().toSimpleString() },
            { "userId", user->getId().toSimpleString() },
            { "ttl", QString::number(ttl) } },
        callback,
        targetThread,
        camera->getParentId());
}

Handle ServerConnection::extendVirtualCameraLock(
    const QnNetworkResourcePtr& camera,
    const QnUserResourcePtr& user,
    const QnUuid& token,
    qint64 ttl,
    GetCallback callback,
    QThread* targetThread)
{
    return executePost(
        "/api/virtualCamera/extend",
        nx::network::rest::Params{
            { "cameraId", camera->getId().toSimpleString() },
            { "token", token.toSimpleString() },
            { "userId", user->getId().toSimpleString() },
            { "ttl", QString::number(ttl) } },
        callback,
        targetThread,
        camera->getParentId());
}

Handle ServerConnection::releaseVirtualCameraLock(
    const QnNetworkResourcePtr& camera,
    const QnUuid& token,
    GetCallback callback,
    QThread* targetThread)
{
    return executePost(
        "/api/virtualCamera/release",
        nx::network::rest::Params{
            { "cameraId", camera->getId().toSimpleString() },
            { "token", token.toSimpleString() } },
        callback,
        targetThread,
        camera->getParentId());
}

Handle ServerConnection::consumeVirtualCameraFile(
    const QnNetworkResourcePtr& camera,
    const QnUuid& token,
    const QString& uploadId,
    qint64 startTimeMs,
    PostCallback callback,
    QThread* targetThread)
{
    return executePost(
        "/api/virtualCamera/consume",
        nx::network::rest::Params{
            { "cameraId", camera->getId().toSimpleString() },
            { "token", token.toSimpleString() },
            { "uploadId", uploadId },
            { "startTime", QString::number(startTimeMs) } },
        callback,
        targetThread,
        camera->getParentId());
}

Handle ServerConnection::getStatistics(
    const QnUuid& serverId,
    ServerConnection::GetCallback callback,
    QThread* targetThread)
{
    return executeGet("/api/statistics", {}, callback, targetThread, serverId);
}

Handle ServerConnection::getAuditLogRecords(
    std::chrono::milliseconds from,
    std::chrono::milliseconds to,
    UbJsonResultCallback callback,
    QThread* targetThread,
    std::optional<QnUuid> proxyToServer)
{
    return getUbJsonResult(
        "/api/auditLog",
        {
            {"from", QString::number(from.count())},
            {"to", QString::number(to.count())},
        },
        std::move(callback),
        targetThread,
        proxyToServer);
}

Handle ServerConnection::getEvents(
    const QnUuid& serverId,
    QnEventLogRequestData request,
    Result<EventLogData>::type callback,
    QThread* targetThread)
{
    request.format = Qn::SerializationFormat::ubjson;
    return executeGet("/api/getEvents", request.toParams(), callback, targetThread, serverId);
}

Handle ServerConnection::getEvents(const QnEventLogMultiserverRequestData& request,
    Result<EventLogData>::type callback,
    QThread* targetThread)
{
    return executeGet(lit("/ec2/getEvents"), request.toParams(), callback, targetThread);
}

Handle ServerConnection::eventLog(
    const nx::vms::api::rules::EventLogFilter& filter,
    Result<ErrorOrData<nx::vms::api::rules::EventLogRecordList>>::type callback,
    QThread* targetThread,
    std::optional<Timeouts> timeouts)
{
    QJsonValue value;
    QJson::serialize(filter, &value);
    NX_ASSERT(value.isObject());
    return executeGet(
        "rest/v3/events/log",
        nx::network::rest::Params::fromJson(value.toObject()),
        std::move(callback),
        targetThread,
        /*proxyToServer*/{},
        timeouts);
}

Handle ServerConnection::getCameraCredentials(
    const QnUuid& deviceId,
    Result<QAuthenticator>::type callback,
    QThread* targetThread)
{
    return executeGet(
        nx::format("/rest/v1/devices/%1", deviceId),
        nx::network::rest::Params{{"_with", "credentials"}},
        Result<QByteArray>::type(
            [callback = std::move(callback)](
                bool success,
                Handle requestId,
                QByteArray result,
                const nx::network::http::HttpHeaders& /*headers*/)
            {
                nx::vms::api::DeviceModel resultObject;

                static const auto kMaskedPassword = QLatin1String("******");

                if (success)
                {
                    success = QJson::deserialize(result, &resultObject)
                        && resultObject.credentials.has_value()
                        && resultObject.credentials->password != kMaskedPassword;
                }

                QAuthenticator credentials;
                if (success)
                {
                    credentials.setUser(resultObject.credentials->user);
                    credentials.setPassword(resultObject.credentials->password);
                }

                callback(
                    success,
                    requestId,
                    credentials);
            }),
        targetThread);
}

Handle ServerConnection::changeCameraPassword(
    const QnVirtualCameraResourcePtr& camera,
    const QAuthenticator& auth,
    Result<ErrorOrEmpty>::type callback,
    QThread* targetThread)
{
    if (!camera || camera->getParentId().isNull())
        return Handle();

    nx::vms::api::DevicePasswordRequest request;
    request.user = auth.user();
    request.password = auth.password();

    return executePost(
        nx::format("/rest/v1/devices/%1/changePassword", camera->getId()),
        nx::reflect::json::serialize(request),
        std::move(callback),
        targetThread);
}

int ServerConnection::checkCameraList(
    const QnUuid& serverId,
    const QnNetworkResourceList& cameras,
    Result<QnCameraListReply>::type callback,
    QThread* targetThread)
{
    QnCameraListReply camList;
    for (const auto& c: cameras)
        camList.physicalIdList << c->getPhysicalId();

    return executePost(
        "/api/checkDiscovery",
        QJson::serialized(camList),
        extractJsonResult<QnCameraListReply>(std::move(callback)),
        targetThread,
        serverId);

}

Handle ServerConnection::lookupObjectTracks(
    const nx::analytics::db::Filter& request,
    bool isLocal,
    Result<nx::analytics::db::LookupResult>::type callback,
    QThread* targetThread,
    std::optional<QnUuid> proxyToServer)
{
    nx::network::rest::Params queryParams;
    nx::analytics::db::serializeToParams(request, &queryParams);
    queryParams.insert("isLocal", isLocal? "true" : "false");

    return executeGet(
        "/ec2/analyticsLookupObjectTracks",
        queryParams,
        callback,
        targetThread,
        proxyToServer);
}

//--------------------------------------------------------------------------------------------------

Handle ServerConnection::getEngineAnalyticsSettings(
    const nx::vms::common::AnalyticsEngineResourcePtr& engine,
    Result<nx::vms::api::analytics::EngineSettingsResponse>::type&& callback,
    QThread* targetThread)
{
    return executeGet(
        "/ec2/analyticsEngineSettings",
        nx::network::rest::Params{
            {"analyticsEngineId", engine->getId().toString()}
        },
        extractJsonResult<nx::vms::api::analytics::EngineSettingsResponse>(std::move(callback)),
        targetThread);
}

Handle ServerConnection::setEngineAnalyticsSettings(
    const nx::vms::common::AnalyticsEngineResourcePtr& engine,
    const QJsonObject& settings,
    Result<nx::vms::api::analytics::EngineSettingsResponse>::type&& callback,
    QThread* targetThread)
{
    nx::vms::api::analytics::EngineSettingsRequest request;
    request.settingsValues = settings;
    request.analyticsEngineId = engine->getId();
    // TODO: Fill settingsModelId when it is supported by the Client.
    using namespace nx::vms::api::analytics;
    return executePost<nx::network::rest::JsonResult>(
        "/ec2/analyticsEngineSettings",
        QJson::serialized(request),
        extractJsonResult<EngineSettingsResponse>(std::move(callback)),
        targetThread);
}

Handle ServerConnection::engineAnalyticsActiveSettingsChanged(
    const nx::vms::common::AnalyticsEngineResourcePtr& engine,
    const QString& activeElement,
    const QJsonObject& settingsModel,
    const QJsonObject& settingsValues,
    const QJsonObject& paramValues,
    Result<nx::vms::api::analytics::EngineActiveSettingChangedResponse>::type&& callback,
    QThread* targetThread)
{
    nx::vms::api::analytics::EngineActiveSettingChangedRequest request;
    request.analyticsEngineId = engine->getId();
    request.activeSettingName = activeElement;
    request.settingsModel = settingsModel;
    request.settingsValues = settingsValues;
    request.paramValues = paramValues;

    using namespace nx::vms::api::analytics;
    return executePost<nx::network::rest::JsonResult>(
        "/ec2/notifyAnalyticsEngineActiveSettingChanged",
        QJson::serialized(request),
        extractJsonResult<EngineActiveSettingChangedResponse>(std::move(callback)),
        targetThread);
}

Handle ServerConnection::getDeviceAnalyticsSettings(
    const QnVirtualCameraResourcePtr& device,
    const nx::vms::common::AnalyticsEngineResourcePtr& engine,
    Result<nx::vms::api::analytics::DeviceAgentSettingsResponse>::type&& callback,
    QThread* targetThread)
{
    using namespace nx::vms::api::analytics;
    return executeGet(
        "/ec2/deviceAnalyticsSettings",
        nx::network::rest::Params{
            {"deviceId", device->getId().toString()},
            {"analyticsEngineId", engine->getId().toString()},
        },
        extractJsonResult<DeviceAgentSettingsResponse>(std::move(callback)),
        targetThread);
}

Handle ServerConnection::setDeviceAnalyticsSettings(
    const QnVirtualCameraResourcePtr& device,
    const nx::vms::common::AnalyticsEngineResourcePtr& engine,
    const QJsonObject& settings,
    const QnUuid& settingsModelId,
    Result<nx::vms::api::analytics::DeviceAgentSettingsResponse>::type&& callback,
    QThread* targetThread)
{
    nx::vms::api::analytics::DeviceAgentSettingsRequest request;
    request.settingsValues = settings;
    request.settingsModelId = settingsModelId;
    request.analyticsEngineId = engine->getId();
    request.deviceId = device->getId().toString();

    return executePost<nx::network::rest::JsonResult>(
        "/ec2/deviceAnalyticsSettings",
        QJson::serialized(request),
        extractJsonResult<nx::vms::api::analytics::DeviceAgentSettingsResponse>(std::move(callback)),
        targetThread);
}

Handle ServerConnection::deviceAnalyticsActiveSettingsChanged(
    const QnVirtualCameraResourcePtr& device,
    const nx::vms::common::AnalyticsEngineResourcePtr& engine,
    const QString& activeElement,
    const QJsonObject& settingsModel,
    const QJsonObject& settingsValues,
    const QJsonObject& paramValues,
    Result<nx::vms::api::analytics::DeviceAgentActiveSettingChangedResponse>::type&& callback,
    QThread* targetThread)
{
    nx::vms::api::analytics::DeviceAgentActiveSettingChangedRequest request;
    request.analyticsEngineId = engine->getId();
    request.deviceId = device->getId().toString();
    request.activeSettingName = activeElement;
    request.settingsModel = settingsModel;
    request.settingsValues = settingsValues;
    request.paramValues = paramValues;

    return executePost<nx::network::rest::JsonResult>(
        "/ec2/notifyDeviceAnalyticsActiveSettingChanged",
        QJson::serialized(request),
        extractJsonResult<nx::vms::api::analytics::DeviceAgentActiveSettingChangedResponse>(
            std::move(callback)),
        targetThread);
}

Handle ServerConnection::startArchiveRebuild(const QnUuid& serverId,
    const QString pool,
    Result<ErrorOrData<nx::vms::api::StorageScanInfoFull>>::type&& callback,
    QThread* targetThread)
{
    const auto endpoint =
        NX_FMT("/rest/v2/servers/%1/rebuildArchive/%2", serverId, pool);
    return executePost(endpoint, nx::network::rest::Params(), std::move(callback), targetThread);
}

Handle ServerConnection::getArchiveRebuildProgress(const QnUuid& serverId,
    const QString pool,
    Result<ErrorOrData<nx::vms::api::StorageScanInfoFull>>::type&& callback,
    QThread* targetThread)
{
    const auto endpoint =
        NX_FMT("/rest/v2/servers/%1/rebuildArchive/%2", serverId, pool);
    return executeGet(endpoint,
        nx::network::rest::Params{{"_keepDefault", QnLexical::serialized(true)}},
        std::move(callback),
        targetThread);
}

Handle ServerConnection::stopArchiveRebuild(const QnUuid& serverId,
    const QString pool,
    Result<ErrorOrEmpty>::type&& callback,
    QThread* targetThread)
{
    const auto endpoint =
        NX_FMT("/rest/v2/servers/%1/rebuildArchive/%2", serverId, pool);
    return executeDelete(endpoint, nx::network::rest::Params(), std::move(callback), targetThread);
}

Handle ServerConnection::postJsonResult(
    const QString& action,
    const nx::network::rest::Params& params,
    const QByteArray& body,
    JsonResultCallback&& callback,
    QThread* targetThread,
    std::optional<Timeouts> timeouts,
    std::optional<QnUuid> proxyToServer)
{
    const auto contentType = Qn::serializationFormatToHttpContentType(Qn::SerializationFormat::json);
    return executePost<nx::network::rest::JsonResult>(
        action,
        params,
        contentType,
        body,
        callback,
        targetThread,
        timeouts,
        proxyToServer);
}

using JsonRpcResultType = std::variant<
    nx::vms::api::JsonRpcResponse,
    std::vector<nx::vms::api::JsonRpcResponse>>;

using JsonRpcRequestIdType = decltype(nx::vms::api::JsonRpcRequest::id);
using JsonRpcResponseIdType = decltype(nx::vms::api::JsonRpcResponse::id);

std::tuple<
    std::unordered_set<JsonRpcRequestIdType>,
    std::vector<nx::vms::api::JsonRpcResponse>>
extractJsonRpcExpired(const rest::ErrorOrData<JsonRpcResultType>& result)
{
    const auto rpcResponse = std::get_if<JsonRpcResultType>(&result);
    if (!rpcResponse)
        return {};

    const auto responseArray = std::get_if<
        std::vector<nx::vms::api::JsonRpcResponse>>(rpcResponse);

    if (!responseArray)
        return {};

    std::unordered_set<JsonRpcRequestIdType> ids;

    for (const auto& response: *responseArray)
    {
        if (isSessionExpiredError(response))
        {
            if (const auto intId = std::get_if<int>(&response.id))
                ids.insert(*intId);
            else if (const auto strId = std::get_if<QString>(&response.id))
                ids.insert(*strId);
        }
    }

    return {std::move(ids), *responseArray};
}

bool mergeJsonRpcResults(
    std::vector<nx::vms::api::JsonRpcResponse>& originalResponse,
    const rest::ErrorOrData<JsonRpcResultType>& result)
{
    const auto rpcResponse = std::get_if<JsonRpcResultType>(&result);
    if (!rpcResponse)
    {
        // Server could not handle the request.
        if (const auto error = std::get_if<nx::network::rest::Result>(&result))
        {
            QJsonValue data;
            QJson::serialize(error, &data);

            // For all requests with expired session fill in error from single rest::Result.
            for (auto& response: originalResponse)
            {
                if (isSessionExpiredError(response))
                {
                    response.result = {};
                    response.error = vms::api::JsonRpcError{
                        .code = nx::vms::api::JsonRpcError::RequestError,
                        .data = data
                    };
                }
            }
            return true;
        }
        return false;
    }

    const auto responseArray = std::get_if<std::vector<nx::vms::api::JsonRpcResponse>>(rpcResponse);
    if (!responseArray)
    {
        // This should not happen because original requests were valid. But handle it anyway.

        if (const auto error = std::get_if<nx::vms::api::JsonRpcResponse>(rpcResponse))
        {
            // For all requests with expired session fill in error from single json-rpc response.
            for (auto& response: originalResponse)
            {
                if (isSessionExpiredError(response))
                {
                    response.result = {};
                    response.error = error->error;
                }
            }
            return true;
        }
        return false;
    }

    // Build a map for faster response replacement.
    std::unordered_map<JsonRpcResponseIdType, const nx::vms::api::JsonRpcResponse*> idToResponse;

    for (const auto& response: *responseArray)
    {
        if (!std::holds_alternative<std::nullptr_t>(response.id))
            idToResponse.insert({response.id, &response});
    }

    std::vector<nx::vms::api::JsonRpcResponse> updatedResponses;

    for (auto& response: originalResponse)
    {
        // Replace original response with the new one if it has the same id.
        if (const auto it = idToResponse.find(response.id); it != idToResponse.end())
            response = *it->second;
    }

    return true;
}

Handle ServerConnection::jsonRpcBatchCall(
    nx::vms::common::SessionTokenHelperPtr helper,
    const std::vector<nx::vms::api::JsonRpcRequest>& requests,
    JsonRpcBatchResultCallback&& callback,
    QThread* targetThread,
    std::optional<Timeouts> timeouts)
{
    auto request = prepareRequest(
        nx::network::http::Method::post,
        prepareUrl(kJsonRpcPath, /*params*/ {}),
        Qn::serializationFormatToHttpContentType(Qn::SerializationFormat::json),
        QJson::serialized(requests));

    auto internalCallback =
        [callback = std::move(callback)](
            bool success,
            Handle requestId,
            rest::ErrorOrData<JsonRpcResultType> result)
        {
            if (success)
            {
                if (const auto rpcResponse = std::get_if<JsonRpcResultType>(&result))
                {
                    if (const auto responseArray = std::get_if<
                        std::vector<nx::vms::api::JsonRpcResponse>>(rpcResponse))
                    {
                        callback(success, requestId, *responseArray);
                        return;
                    }
                }
                NX_ASSERT(false, "jsonrpc success but response data is invalid");
                return;
            }

            if (const auto error = std::get_if<nx::network::rest::Result>(&result))
            {
                nx::vms::api::JsonRpcResponse rpcError;

                QJsonValue data;
                QJson::serialize(error, &data);

                rpcError.error = vms::api::JsonRpcError{
                    .code = nx::vms::api::JsonRpcError::RequestError,
                    .data = data
                };

                callback(success, requestId, {rpcError});
                return;
            }

            if (const auto rpcResponse = std::get_if<JsonRpcResultType>(&result))
            {
                if (const auto singleResponse = std::get_if<
                    nx::vms::api::JsonRpcResponse>(rpcResponse))
                {
                    callback(success, requestId, {*singleResponse});
                }
                return;
            }

            callback(success, requestId, {});
        };

    auto wrapper = makeSessionAwareCallbackInternal<
        rest::ErrorOrData<JsonRpcResultType>, rest::ErrorOrData<JsonRpcResultType>>(
            helper,
            request,
            std::move(internalCallback),
            timeouts,
            requests);

    auto handle = request.isValid()
        ? executeRequest(request, std::move(wrapper), targetThread, timeouts)
        : Handle();

    NX_VERBOSE(d->logTag, "<%1> %2", handle, request.url);
    return handle;
}

Handle ServerConnection::postEmptyResult(
    const QString& action,
    const nx::network::rest::Params& params,
    const QByteArray& body,
    PostCallback&& callback,
    QThread* targetThread,
    std::optional<QnUuid> proxyToServer,
    std::optional<Qn::SerializationFormat> contentType)
{
    const auto contentTypeString
        = contentType ? Qn::serializationFormatToHttpContentType(*contentType) : "";

    return executePost<EmptyResponseType>(
        action,
        params,
        contentTypeString,
        body,
        callback,
        targetThread,
        /*timeouts*/ {},
        proxyToServer);
}

Handle ServerConnection::putEmptyResult(
    const QString& action,
    const nx::network::rest::Params& params,
    const QByteArray& body,
    PostCallback&& callback,
    QThread* targetThread,
    std::optional<QnUuid> proxyToServer)
{
    const auto contentType = Qn::serializationFormatToHttpContentType(Qn::SerializationFormat::json);
    return executePut<EmptyResponseType>(
        action,
        params,
        contentType,
        body,
        callback,
        targetThread,
        proxyToServer);
}

Handle ServerConnection::putRest(
    nx::vms::common::SessionTokenHelperPtr helper,
    const QString &action,
    const nx::network::rest::Params &params,
    const QByteArray &body,
    Result<ErrorOrEmpty>::type callback,
    nx::utils::AsyncHandlerExecutor executor)
{
    auto request = prepareRequest(
        nx::network::http::Method::put,
        prepareUrl(action, params),
        Qn::serializationFormatToHttpContentType(Qn::SerializationFormat::json),
        body);

    auto wrapper = makeSessionAwareCallback(helper, request, std::move(callback));

    auto handle = request.isValid()
        ? executeRequest(request, std::move(wrapper), executor)
        : Handle();

    return handle;
}

Handle ServerConnection::patchRest(
    nx::vms::common::SessionTokenHelperPtr helper,
    const QString& action,
    const nx::network::rest::Params& params,
    const nx::String& body,
    Result<ErrorOrData<QByteArray>>::type callback,
    nx::utils::AsyncHandlerExecutor executor)
{
    auto request = prepareRequest(nx::network::http::Method::patch,
        prepareUrl(action, params),
        Qn::serializationFormatToHttpContentType(Qn::SerializationFormat::json),
        body);

    auto wrapper = makeSessionAwareCallback(helper, request, std::move(callback));

    auto handle =
        request.isValid() ? executeRequest(request, std::move(wrapper), executor) : Handle();

    return handle;
}

Handle ServerConnection::postRest(nx::vms::common::SessionTokenHelperPtr helper,
    const QString& action,
    const nx::network::rest::Params& params,
    const QByteArray& body,
    Result<ErrorOrEmpty>::type callback,
    nx::utils::AsyncHandlerExecutor executor)
{
    auto request = prepareRequest(nx::network::http::Method::post,
        prepareUrl(action, params),
        Qn::serializationFormatToHttpContentType(Qn::SerializationFormat::json),
        body);

    auto wrapper = makeSessionAwareCallback(helper, request, std::move(callback));

    auto handle =
        request.isValid() ? executeRequest(request, std::move(wrapper), executor) : Handle();

    return handle;
}

Handle ServerConnection::getUbJsonResult(
    const QString& path,
    nx::network::rest::Params params,
    UbJsonResultCallback&& callback,
    QThread* targetThread,
    std::optional<QnUuid> proxyToServer)
{
    if (!params.contains("format"))
        params.insert("format", "ubjson");
    return executeGet(path, params, callback, targetThread, proxyToServer);
}

Handle ServerConnection::getJsonResult(
    const QString& path,
    nx::network::rest::Params params,
    JsonResultCallback&& callback,
    QThread* targetThread,
    std::optional<QnUuid> proxyToServer)
{
    if (!params.contains("format"))
        params.insert("format", "json");
    return executeGet(path, params, callback, targetThread, proxyToServer);
}

Handle ServerConnection::getRawResult(
    const QString& path,
    const nx::network::rest::Params& params,
    Result<QByteArray>::type callback,
    QThread* targetThread)
{
    return executeGet(path, params, callback, targetThread);
}

Handle ServerConnection::deleteEmptyResult(
    const QString& action,
    const nx::network::rest::Params& params,
    PostCallback&& callback,
    QThread* targetThread)
{
    return executeDelete<EmptyResponseType>(
        action,
        params,
        callback,
        targetThread);
}

Handle ServerConnection::deleteRest(
    nx::vms::common::SessionTokenHelperPtr helper,
    const QString &action,
    const nx::network::rest::Params &params,
    Result<ErrorOrEmpty>::type callback,
    nx::utils::AsyncHandlerExecutor executor)
{
    auto request = prepareRequest(
        nx::network::http::Method::delete_,
        prepareUrl(action, params));

    auto wrapper = makeSessionAwareCallback(std::move(helper), request, std::move(callback));

    auto handle = request.isValid()
        ? executeRequest(request, std::move(wrapper), std::move(executor))
        : Handle();

    return handle;
}

Handle ServerConnection::postUbJsonResult(
    const QString& action,
    const nx::network::rest::Params& params,
    const QByteArray& body,
    UbJsonResultCallback&& callback,
    QThread* targetThread)
{
    const auto contentType = Qn::serializationFormatToHttpContentType(Qn::SerializationFormat::ubjson);
    return executePost<nx::network::rest::UbjsonResult>(action,
        params,
        contentType,
        body,
        callback,
        targetThread);
}

Handle ServerConnection::getPluginInformation(
    const QnUuid& serverId,
    GetCallback callback,
    QThread* targetThread)
{
    return executeGet("/api/pluginInfo", {}, callback, targetThread, serverId);
}

Handle ServerConnection::testEmailSettings(
    const nx::vms::api::EmailSettings& settings,
    Result<RestResultWithData<QnTestEmailSettingsReply>>::type&& callback,
    QThread* targetThread,
    std::optional<QnUuid> proxyToServer)
{
    return executePost(
        "/api/testEmailSettings",
        QJson::serialized(settings),
        std::move(callback),
        targetThread,
        proxyToServer);
}

Handle ServerConnection::testEmailSettings(
    Result<RestResultWithData<QnTestEmailSettingsReply>>::type&& callback,
    QThread* targetThread,
    std::optional<QnUuid> proxyToServer)
{
    return executePost(
        "/api/testEmailSettings",
        /*messageBody*/ QByteArray(),
        std::move(callback),
        targetThread,
        proxyToServer);
}

Handle ServerConnection::getStorageStatus(
    const QnUuid& serverId,
    const QString& path,
    Result<RestResultWithData<StorageStatusReply>>::type&& callback,
    QThread* targetThread)
{
    nx::network::rest::Params params;
    params.insert("path", path);
    return executeGet("/api/storageStatus", params, callback, targetThread, serverId);
}

Handle ServerConnection::setStorageEncryptionPassword(
    const QString& password,
    bool makeCurrent,
    const QByteArray& salt,
    PostCallback&& callback,
    QThread* targetThread)
{
    nx::vms::api::StorageEncryptionData data;
    data.password = password;
    data.makeCurrent = makeCurrent;
    data.salt = salt;

    return executePost(
        "/rest/v1/system/storageEncryption",
        QJson::serialized(data),
        std::move(callback),
        targetThread);
}

Handle ServerConnection::getSystemIdFromServer(
    const QnUuid& serverId,
    Result<QString>::type&& callback,
    QThread* targetThread)
{
    auto internalCallback =
        [callback=std::move(callback)](
            bool success, Handle requestId, QByteArray result,
            const nx::network::http::HttpHeaders& /*headers*/)
        {
            callback(success, requestId, QString::fromUtf8(result));
        };
    return executeGet("/api/getSystemId", {}, internalCallback, targetThread, serverId);
}

Handle ServerConnection::doCameraDiagnosticsStep(
    const QnUuid& serverId,
    const QnUuid& cameraId,
    CameraDiagnostics::Step::Value previousStep,
    Result<RestResultWithData<QnCameraDiagnosticsReply>>::type&& callback,
    QThread* targetThread)
{
    nx::network::rest::Params params;
    params.insert("cameraId", cameraId);
    params.insert("type", CameraDiagnostics::Step::toString(previousStep));

    return executeGet("/api/doCameraDiagnosticsStep", params, callback, targetThread, serverId);
}

template<class ResultType>
std::function<void(ServerConnection::ContextPtr context)> makeCallbackWithErrorMessage(
    std::function<void(
        bool success,
        Handle requestId,
        const ResultType& result,
        const QString& message)> callback, QnUuid serverId)
{
    return [callback, serverId](ServerConnection::ContextPtr context)
        {
            bool success = false;
            const auto format = Qn::serializationFormatFromHttpContentType(context->response.contentType);
            bool goodFormat = format == Qn::SerializationFormat::json || format == Qn::SerializationFormat::ubjson;

            std::shared_ptr<ResultType> result;

            QString errorString;
            if (context->systemError != SystemError::noError
                || context->getStatusCode() != nx::network::http::StatusCode::ok)
            {
                success = false;
            }

            if (success && goodFormat)
            {
                nx::network::rest::JsonResult restResult;
                if (!QJson::deserialize(context->response.messageBody, &restResult))
                    success = false;

                //else
                //    QJson::deserialize(restResult.reply, &result);
            }

            auto internal_callback =
                [callback, success, id = context->handle, result, errorString]()
                {
                    if (callback)
                        callback(success, id, result ? std::move(*result) : ResultType(), errorString);
                };

            invoke(context, internal_callback, success, serverId.toString());
        };
}

Handle ServerConnection::ldapAuthenticateAsync(
    const nx::vms::api::Credentials& credentials,
    LdapAuthenticateCallback&& callback,
    QThread* targetThread)
{
    auto request = prepareRequest(
        nx::network::http::Method::post,
        prepareUrl(
            "/rest/v3/ldap/authenticate",
            /*params*/ {}),
        nx::network::http::header::ContentType::kJson.toString(),
        nx::reflect::json::serialize(credentials));

    auto handle = request.isValid()
        ? executeRequest(
            request,
            [callback = std::move(callback)](
                bool success,
                Handle requestId,
                QByteArray body,
                const nx::network::http::HttpHeaders& httpHeaders)
            {
                using AuthResult = nx::vms::common::AuthResult;
                AuthResult authResult = AuthResult::Auth_LDAPConnectError;
                const auto authResultString = nx::network::http::getHeaderValue(
                    httpHeaders, Qn::AUTH_RESULT_HEADER_NAME);
                if (!authResultString.empty())
                    nx::reflect::fromString<AuthResult>(authResultString, &authResult);
                if (!success)
                {
                    nx::network::rest::Result result;
                    QJson::deserialize(body, &result);
                    callback(requestId, std::move(result), authResult);
                    return;
                }

                nx::vms::api::UserModelV3 user;
                QJson::deserialize(body, &user);
                callback(requestId, std::move(user), authResult);
            },
            targetThread)
        : Handle();

    NX_VERBOSE(d->logTag, "<%1> %2", handle, request.url);
    return handle;
}

Handle ServerConnection::testLdapSettingsAsync(
    const nx::vms::api::LdapSettings& settings,
    Result<ErrorOrData<std::vector<QString>>>::type&& callback,
    QThread* targetThread)
{
    return executePost(
        "/rest/v3/ldap/test",
        nx::reflect::json::serialize(settings),
        std::move(callback),
        targetThread);
}

Handle ServerConnection::setLdapSettingsAsync(
    const nx::vms::api::LdapSettings& settings,
    nx::vms::common::SessionTokenHelperPtr helper,
    Result<ErrorOrData<nx::vms::api::LdapSettings>>::type&& callback,
    QThread* targetThread)
{
    auto request = prepareRequest(
        nx::network::http::Method::put,
        prepareUrl(
            "/rest/v3/ldap/settings",
            /*params*/ {}),
        Qn::serializationFormatToHttpContentType(Qn::SerializationFormat::json),
        nx::reflect::json::serialize(settings));

    auto wrapper = makeSessionAwareCallback(helper, request, std::move(callback));

    auto handle = request.isValid()
        ? executeRequest(request, std::move(wrapper), targetThread)
        : Handle();

    NX_VERBOSE(d->logTag, "<%1> %2", handle, request.url);
    return handle;
}

Handle ServerConnection::modifyLdapSettingsAsync(
    const nx::vms::api::LdapSettings& settings,
    nx::vms::common::SessionTokenHelperPtr helper,
    Result<ErrorOrData<nx::vms::api::LdapSettings>>::type&& callback,
    QThread* targetThread)
{
    auto request = prepareRequest(
        nx::network::http::Method::patch,
        prepareUrl(
            "/rest/v3/ldap/settings",
            /*params*/ {}),
        Qn::serializationFormatToHttpContentType(Qn::SerializationFormat::json),
        nx::reflect::json::serialize(settings));

    auto wrapper = makeSessionAwareCallback(helper, request, std::move(callback));

    auto handle = request.isValid()
        ? executeRequest(request, std::move(wrapper), targetThread)
        : Handle();

    NX_VERBOSE(d->logTag, "<%1> %2", handle, request.url);
    return handle;
}

Handle ServerConnection::loginInfoAsync(
    const QString& login,
    Result<ErrorOrData<nx::vms::api::LoginUser>>::type&& callback,
    QThread* targetThread)
{
    return executeGet("/rest/v3/login/users/" + login, {}, callback, targetThread);
}

Handle ServerConnection::getLdapSettingsAsync(
    Result<ErrorOrData<nx::vms::api::LdapSettings>>::type&& callback,
    QThread* targetThread)
{
    return executeGet("/rest/v3/ldap/settings", {}, callback, targetThread);
}

Handle ServerConnection::getLdapStatusAsync(
    Result<ErrorOrData<nx::vms::api::LdapStatus>>::type&& callback,
    QThread* targetThread)
{
    return executeGet("/rest/v3/ldap/sync", {}, callback, targetThread);
}

Handle ServerConnection::syncLdapAsync(
    nx::vms::common::SessionTokenHelperPtr helper,
    Result<ErrorOrEmpty>::type&& callback,
    QThread* targetThread)
{
    auto request = prepareRequest(
        nx::network::http::Method::post,
        prepareUrl(
            "/rest/v3/ldap/sync",
            /*params*/ {}),
        Qn::serializationFormatToHttpContentType(Qn::SerializationFormat::json));

    auto wrapper = makeSessionAwareCallback(helper, request, std::move(callback));

    auto handle = request.isValid()
        ? executeRequest(request, std::move(wrapper), targetThread)
        : Handle();

    NX_VERBOSE(d->logTag, "<%1> %2", handle, request.url);
    return handle;
}

Handle ServerConnection::resetLdapAsync(
    nx::vms::common::SessionTokenHelperPtr helper,
    Result<ErrorOrEmpty>::type&& callback,
    QThread* targetThread)
{
    auto request = prepareRequest(
        nx::network::http::Method::delete_,
        prepareUrl(
            "/rest/v3/ldap/settings",
            /*params*/ {}),
        Qn::serializationFormatToHttpContentType(Qn::SerializationFormat::json));

    auto wrapper = makeSessionAwareCallback(helper, request, std::move(callback));

    auto handle = request.isValid()
        ? executeRequest(request, std::move(wrapper), targetThread)
        : Handle();

    NX_VERBOSE(d->logTag, "<%1> %2", handle, request.url);
    return handle;
}

Handle ServerConnection::saveUserAsync(
    bool newUser,
    const nx::vms::api::UserModelV3& userData,
    nx::vms::common::SessionTokenHelperPtr helper,
    Result<ErrorOrData<nx::vms::api::UserModelV3>>::type&& callback,
    QThread* targetThread)
{
    auto request = prepareRequest(
        newUser ? nx::network::http::Method::put : nx::network::http::Method::patch,
        prepareUrl(
            QString("/rest/v3/users/%1").arg(userData.getId().toString()),
            /*params*/ {}),
        Qn::serializationFormatToHttpContentType(Qn::SerializationFormat::json),
        nx::reflect::json::serialize(userData));

    auto wrapper = makeSessionAwareCallback(helper, request, std::move(callback));

    auto handle = request.isValid()
        ? executeRequest(request, std::move(wrapper), targetThread)
        : Handle();

    NX_VERBOSE(d->logTag, "<%1> %2", handle, request.url);
    return handle;
}

Handle ServerConnection::removeUserAsync(
    const QnUuid& userId,
    nx::vms::common::SessionTokenHelperPtr helper,
    Result<ErrorOrEmpty>::type&& callback,
    QThread* targetThread)
{
    auto request = prepareRequest(
        nx::network::http::Method::delete_,
        prepareUrl(
            QString("/rest/v3/users/%1").arg(userId.toString()),
            /*params*/ {}));

    auto wrapper = makeSessionAwareCallback(helper, request, std::move(callback));

    auto handle = request.isValid()
        ? executeRequest(request, std::move(wrapper), targetThread)
        : Handle();

    NX_VERBOSE(d->logTag, "<%1> %2", handle, request.url);
    return handle;
}

Handle ServerConnection::saveGroupAsync(
    bool newGroup,
    const nx::vms::api::UserGroupModel& groupData,
    nx::vms::common::SessionTokenHelperPtr helper,
    Result<ErrorOrData<nx::vms::api::UserGroupModel>>::type&& callback,
    QThread* targetThread)
{
    auto request = prepareRequest(
        newGroup ? nx::network::http::Method::put : nx::network::http::Method::patch,
        prepareUrl(
            QString("/rest/v3/userGroups/%1").arg(groupData.id.toString()),
            /*params*/ {}),
        Qn::serializationFormatToHttpContentType(Qn::SerializationFormat::json),
        nx::reflect::json::serialize(groupData));

    auto wrapper = makeSessionAwareCallback(helper, request, std::move(callback));

    auto handle = request.isValid()
        ? executeRequest(request, std::move(wrapper), targetThread)
        : Handle();

    NX_VERBOSE(d->logTag, "<%1> %2", handle, request.url);
    return handle;
}

Handle ServerConnection::removeGroupAsync(
    const QnUuid& groupId,
    nx::vms::common::SessionTokenHelperPtr helper,
    Result<ErrorOrEmpty>::type&& callback,
    QThread* targetThread)
{
    auto request = prepareRequest(
        nx::network::http::Method::delete_,
        prepareUrl(
            QString("/rest/v3/userGroups/%1").arg(groupId.toString()),
            /*params*/ {}));

    auto wrapper = makeSessionAwareCallback(helper, request, std::move(callback));

    auto handle = request.isValid()
        ? executeRequest(request, std::move(wrapper), targetThread)
        : Handle();

    NX_VERBOSE(d->logTag, "<%1> %2", handle, request.url);
    return handle;
}

Handle ServerConnection::loginAsync(
    const nx::vms::api::LoginSessionRequest& data,
    Result<ErrorOrData<nx::vms::api::LoginSession>>::type callback,
    QThread* targetThread)
{
    return executePost(
        "/rest/v1/login/sessions",
        nx::reflect::json::serialize(data),
        std::move(callback),
        targetThread);
}

Handle ServerConnection::loginAsync(
    const nx::vms::api::TemporaryLoginSessionRequest& data,
    Result<ErrorOrData<nx::vms::api::LoginSession>>::type callback,
    QThread* targetThread)
{
    return executePost(
        "/rest/v3/login/temporaryToken",
        nx::reflect::json::serialize(data),
        std::move(callback),
        targetThread);
}

Handle ServerConnection::replaceDevice(
    const QnUuid& deviceToBeReplacedId,
    const QString& replacementDevicePhysicalId,
    bool returnReportOnly,
    Result<nx::vms::api::DeviceReplacementResponse>::type&& callback,
    QThread* targetThread)
{
    if (!NX_ASSERT(
        !deviceToBeReplacedId.isNull() && !replacementDevicePhysicalId.isEmpty(),
        "Invalid parameters"))
    {
        return Handle();
    }

    nx::vms::api::DeviceReplacementRequest requestData;
    requestData.id = deviceToBeReplacedId;
    requestData.replaceWithDeviceId = replacementDevicePhysicalId;
    requestData.dryRun = returnReportOnly;

    auto internal_callback =
        [callback = std::move(callback)]
        (bool success, Handle handle, QByteArray messageBody,
            const nx::network::http::HttpHeaders& headers)
        {
            nx::vms::api::DeviceReplacementResponse response;
            if (success)
                success = nx::reflect::json::deserialize(messageBody.data(), &response).success;
            callback(success, handle, response);
        };

    auto request = prepareRequest(
        nx::network::http::Method::post,
        prepareUrl(nx::format("/rest/v2/devices/%1/replace", deviceToBeReplacedId), /*params*/ {}),
        Qn::serializationFormatToHttpContentType(Qn::SerializationFormat::json),
        QJson::serialized(requestData));

    return executeRequest(request, std::move(internal_callback), targetThread);
}

Handle ServerConnection::undoReplaceDevice(
    const QnUuid& deviceId,
    PostCallback&& callback,
    QThread* targetThread)
{
    return executeDelete(
        nx::format("/rest/v2/devices/%1/replace", deviceId),
        nx::network::rest::Params(),
        callback,
        targetThread,
        {});
}

Handle ServerConnection::recordedTimePeriods(
    const QnChunksRequestData& request,
    Result<MultiServerPeriodDataList>::type&& callback,
    QThread* targetThread)
{
    QnChunksRequestData fixedFormatRequest(request);
    fixedFormatRequest.format = Qn::SerializationFormat::compressedPeriods;
    auto internalCallback =
        [callback=std::move(callback)](
            bool success, Handle requestId, QByteArray result,
            const nx::network::http::HttpHeaders& /*headers*/)
        {
            if (success)
            {
                bool goodData = false;
                auto chunks = QnCompressedTime::deserialized<MultiServerPeriodDataList>(
                    result, {}, &goodData);
                callback(goodData, requestId, chunks);
                return;
            }
            callback(false, requestId, {});
        };
    return executeGet("/ec2/recordedTimePeriods", fixedFormatRequest.toParams(), internalCallback,
        targetThread);
}

Handle ServerConnection::getExtendedPluginInformation(
    Result<nx::vms::api::ExtendedPluginInfoByServer>::type&& callback,
    QThread* targetThread)
{
    return executeGet(
        "/ec2/pluginInfo",
        {},
        Result<nx::network::rest::JsonResult>::type(
            [funcName = __func__, callback = std::move(callback), this](
                bool success, Handle requestId, const nx::network::rest::JsonResult& result)
            {
                nx::vms::api::ExtendedPluginInfoByServer pluginInfo;
                if (!QJson::deserialize(result.reply, &pluginInfo))
                {
                    NX_DEBUG(d->logTag,
                        "%1: Unable to deserialize the response from the Server %2, %3",
                        funcName, d->serverId, QJson::serialize(result.reply));
                }

                callback(
                    success,
                    requestId,
                    std::move(pluginInfo));
            }),
        targetThread);
}

Handle ServerConnection::debug(
    const QString& action, const QString& value, PostCallback callback, QThread* targetThread)
{
    return executeGet("/api/debug", {{action, value}}, callback, targetThread);
}

Handle ServerConnection::getLookupLists(
    Result<ErrorOrData<nx::vms::api::LookupListDataList>>::type&& callback,
    QThread* targetThread)
{
    return executeGet("/rest/v3/lookupLists", {}, std::move(callback), targetThread);
}

// --------------------------- private implementation -------------------------------------

QUrl ServerConnection::prepareUrl(const QString& path, const nx::network::rest::Params& params) const
{
    QUrl result;
    result.setPath(path);
    result.setQuery(params.toUrlQuery());
    return result;
}

template<typename CallbackType>
Handle ServerConnection::executeGet(
    const QString& path,
    const nx::network::rest::Params& params,
    CallbackType callback,
    QThread* targetThread,
    std::optional<QnUuid> proxyToServer,
    std::optional<Timeouts> timeouts)
{
    auto request = this->prepareRequest(nx::network::http::Method::get, prepareUrl(path, params));
    if (proxyToServer)
        proxyRequestUsingServer(request, *proxyToServer);

    auto handle = request.isValid()
        ? this->executeRequest(request, std::move(callback), targetThread, timeouts)
        : Handle();

    NX_VERBOSE(d->logTag, "<%1> %2", handle, request.url);
    return handle;
}

template <typename ResultType>
Handle ServerConnection::executePost(
    const QString& path,
    const nx::network::rest::Params& params,
    Callback<ResultType> callback,
    QThread* targetThread,
    std::optional<QnUuid> proxyToServer)
{
    return executePost(
        path,
        QJson::serialized(params.toJson()),
        std::move(callback),
        targetThread,
        proxyToServer);
}

template <typename ResultType>
Handle ServerConnection::executePost(
    const QString& path,
    const nx::String& messageBody,
    Callback<ResultType>&& callback,
    QThread* targetThread,
    std::optional<QnUuid> proxyToServer)
{
    return executePost(
        path,
        /*params*/ {},
        nx::network::http::header::ContentType::kJson.toString(),
        messageBody,
        std::move(callback),
        targetThread,
        /*timeouts*/ {},
        proxyToServer);
}

template <typename ResultType>
Handle ServerConnection::executePost(
    const QString& path,
    const nx::network::rest::Params& params,
    const nx::String& contentType,
    const nx::String& messageBody,
    Callback<ResultType> callback,
    QThread* targetThread,
    std::optional<Timeouts> timeouts,
    std::optional<QnUuid> proxyToServer)
{
    auto request = this->prepareRequest(
        nx::network::http::Method::post, prepareUrl(path, params), contentType, messageBody);

    if (proxyToServer)
        proxyRequestUsingServer(request, *proxyToServer);

    auto handle = request.isValid()
        ? this->executeRequest(request, std::move(callback), targetThread, timeouts)
        : Handle();

    NX_VERBOSE(d->logTag, "<%1> %2", handle, request.url);
    return handle;
}

template <typename ResultType>
Handle ServerConnection::executePut(
    const QString& path,
    const nx::network::rest::Params& params,
    const nx::String& contentType,
    const nx::String& messageBody,
    Callback<ResultType> callback,
    QThread* targetThread,
    std::optional<QnUuid> proxyToServer)
{
    auto request = prepareRequest(
        nx::network::http::Method::put, prepareUrl(path, params), contentType, messageBody);
    if (proxyToServer)
        proxyRequestUsingServer(request, *proxyToServer);

    auto handle = request.isValid()
        ? executeRequest(request, callback, targetThread)
        : Handle();

    NX_VERBOSE(d->logTag, "<%1> %2", handle, request.url);
    return handle;
}

template <typename ResultType>
Handle ServerConnection::executePatch(
    const QString& path,
    const nx::network::rest::Params& params,
    const nx::String& contentType,
    const nx::String& messageBody,
    Callback<ResultType> callback,
    QThread* targetThread)
{
    auto request = prepareRequest(
        nx::network::http::Method::patch, prepareUrl(path, params), contentType, messageBody);
    auto handle = request.isValid()
        ? executeRequest(request, callback, targetThread)
        : Handle();

    NX_VERBOSE(d->logTag, "<%1> %2", handle, request.url);
    return handle;
}

template <typename ResultType>
Handle ServerConnection::executeDelete(
    const QString& path,
    const nx::network::rest::Params& params,
    Callback<ResultType> callback,
    QThread* targetThread,
    std::optional<QnUuid> proxyToServer)
{
    auto request = prepareRequest(nx::network::http::Method::delete_, prepareUrl(path, params));
    if (proxyToServer)
        proxyRequestUsingServer(request, *proxyToServer);

    auto handle = request.isValid()
        ? executeRequest(request, callback, targetThread)
        : Handle();

    NX_VERBOSE(d->logTag, "<%1> %2", handle, request.url);
    return handle;
}

template<typename ResultType>
nx::network::rest::Result::Error getError(const ResultType& result)
{
    auto error = std::get_if<nx::network::rest::Result>(&result);
    return error ? error->error : nx::network::rest::Result::NoError;
}

nx::network::rest::Result::Error getError(
    const QByteArray& body,
    const nx::network::http::HttpHeaders&)
{
    nx::network::rest::Result result;
    return QJson::deserialize(body, &result)
        ? result.error
        : nx::network::rest::Result::NoError; //< We check 'success' explicitly.
}

// Allows to add extra fields to context struct in template specialization.
template <typename T>
struct WithDataForType
{
};

template <>
struct WithDataForType<rest::ErrorOrData<JsonRpcResultType>>
{
    std::unordered_set<JsonRpcRequestIdType> expiredIds;
    std::vector<nx::vms::api::JsonRpcResponse> originalResponse;
    std::vector<nx::vms::api::JsonRpcRequest> requestData;
};

template<typename ResultType, typename... CallbackParameters, typename... Args>
typename ServerConnectionBase::Result<ResultType>::type ServerConnection::makeSessionAwareCallbackInternal(
    nx::vms::common::SessionTokenHelperPtr helper,
    nx::network::http::ClientPool::Request request,
    typename Result<ResultType>::type callback,
    std::optional<nx::network::http::AsyncClient::Timeouts> timeouts,
    Args&&... args)
{
    // For security reasons, some priviledged API requests can be executed only with a recently
    // issued authorization token. If the token is not fresh enough, such request will fail,
    // returning an error, and the user should be asked to enter his password and authorize again.
    // After that the request is resend with a new authorization token. Since the Client can send
    // several priviledged request simultaneously, they all could fail at once. Only one
    // authorization dialog should be shown in that case. Also it's possible that responses for
    // some failed requests will be delivered after this dialog has been closed (e.g. because of a
    // slow internet connection), and another dialog should not be shown in that case.

    NX_ASSERT(this->thread() == qApp->thread());

    struct InteractionContext: public WithDataForType<ResultType>
    {
        QPointer<ServerConnection> ptr;
        nx::vms::common::SessionTokenHelperPtr helper;
        Private::ReissuedTokenPtr reissuedToken;
        nx::network::http::ClientPool::Request request;
        std::optional<nx::network::http::AsyncClient::Timeouts> timeouts;
        typename Result<ResultType>::type callback;

        QThread* interactionThread = qApp->thread();
        QThread* targetThread = {};
    };
    using InteractionContextPtr = std::shared_ptr<InteractionContext>;

    // A shared "future" variable for the token. It's filled when authorization dialog is shown.
    Private::ReissuedTokenPtr reissuedToken;
    {
        NX_MUTEX_LOCKER lock(&d->mutex);
        reissuedToken = d->reissuedToken;
    }

    InteractionContextPtr ctx{new InteractionContext{
        .ptr = this,
        .helper = std::move(helper),
        .reissuedToken = std::move(reissuedToken),
        .request = std::move(request),
        .timeouts = std::move(timeouts),
        .callback = std::move(callback)
    }};

    if constexpr (std::is_same_v<ResultType, rest::ErrorOrData<JsonRpcResultType>>)
        ctx->requestData = std::move(std::forward<Args>(args)...);

    return
        [ctx](bool success, Handle handle, CallbackParameters... result)
        {
            // This function is executed in the target thread of an API request callback.
            ctx->targetThread = QThread::currentThread();

            bool requestNewSession = false;

            if (success)
            {
                if constexpr (std::is_same_v<ResultType, rest::ErrorOrData<JsonRpcResultType>>)
                {
                    // Some json-rpc requests may fail with SessionExpired, but it is considered
                    // as json-rpc success. Extract ids of failed requests for resending when a new
                    // session token is recived.

                    std::tie(ctx->expiredIds, ctx->originalResponse) =
                        extractJsonRpcExpired(result...);
                    requestNewSession = !ctx->expiredIds.empty();
                }
            }
            else if (const auto error = getError(result...))
            {
                requestNewSession = isSessionExpiredError(error);
            }

            if (requestNewSession)
            {
                // Session is expired. Let's try to issue a new token and resend the request.
                executeInThread(ctx->interactionThread,
                    [ctx, handle, success, result...]
                    {
                        // In some cases this callback could be executed when ServerConnection
                        // instance is already destroyed. Perform a safety check.
                        if (!ctx->ptr)
                            return;

                        // Prepare a function that patches both request and callback and sends the
                        // fixed request with new credentials. This function has "void f(token);"
                        // type for all template instances and therefore all such functions can be
                        // stored in a single queue while the app waits for the user interaction.
                        Private::ResendRequestFunc retry =
                            [ctx, handle, success, result...](
                                std::optional<nx::network::http::AuthToken> token)
                            {
                                if (!token)
                                {
                                    // Token was not updated. Process the original callback.
                                    if (ctx->callback)
                                    {
                                        executeInThread(ctx->targetThread,
                                            [ctx, handle, success, result...]
                                            {
                                                ctx->callback(success, handle, result...);
                                            });
                                    }

                                    return;
                                }

                                // We need to update the request, since it stores the credentials.
                                auto fixedRequest = ctx->request;
                                fixedRequest.credentials->authToken = *token;

                                if constexpr (std::is_same_v<
                                    ResultType,
                                    rest::ErrorOrData<JsonRpcResultType>>)
                                {
                                    // Update message body to resend only failed json-rpc requests.
                                    std::vector<nx::vms::api::JsonRpcRequest> newRequests;
                                    for (const auto& request: ctx->requestData)
                                    {
                                        if (ctx->expiredIds.contains(request.id))
                                            newRequests.emplace_back(request);
                                    }
                                    fixedRequest.messageBody =
                                        nx::reflect::json::serialize(newRequests);
                                }

                                // Make an auxiliary callback that will pass the original request
                                // handle to the caller instead of an unknown-to-the-caller resent
                                // request handle.
                                const auto originalHandle = handle;
                                typename ServerConnectionBase::Result<ResultType>::type fixedCallback =
                                    [ctx, originalHandle](
                                        bool success, Handle handle, CallbackParameters... result)
                                    {
                                        executeInThread(ctx->interactionThread,
                                            [ctx, handle, originalHandle]
                                            {
                                                if (ctx->ptr)
                                                {
                                                    // Request is done. Remove the substitution.
                                                    NX_MUTEX_LOCKER lock(&ctx->ptr->d->mutex);
                                                    ctx->ptr->d->substitutions.erase(
                                                        originalHandle);
                                                }

                                                NX_VERBOSE(
                                                    ctx->ptr
                                                        ? ctx->ptr->d->logTag
                                                        : NX_SCOPE_TAG,
                                                    "Received response for <%1> (re-send of <%2>)",
                                                    handle, originalHandle);
                                            });

                                        if constexpr (std::is_same_v<
                                            ResultType,
                                            rest::ErrorOrData<JsonRpcResultType>>)
                                        {
                                            if (ctx->callback)
                                            {
                                                if (mergeJsonRpcResults(
                                                    ctx->originalResponse,
                                                    result...))
                                                {
                                                    // Even if the new request failed, it is still
                                                    // considered as json-rpc success.
                                                    const bool jsonRpcSuccess = true;
                                                    ctx->callback(
                                                        jsonRpcSuccess,
                                                        originalHandle,
                                                        ctx->originalResponse);
                                                    return;
                                                }
                                            }
                                        }

                                        if (ctx->callback)
                                            ctx->callback(success, originalHandle, result...);
                                    };

                                // Resend the request.
                                const auto handle = ctx->ptr->executeRequest(fixedRequest,
                                    std::move(fixedCallback), ctx->targetThread, ctx->timeouts);

                                NX_VERBOSE(ctx->ptr->d->logTag,
                                    "<%1> Sending <%2>(%3) with updated credentials",
                                    handle, originalHandle, ctx->request.url);

                                {
                                    // Store the handles, so we'll be able to cancel the request.
                                    NX_MUTEX_LOCKER lock(&ctx->ptr->d->mutex);
                                    ctx->ptr->d->substitutions[originalHandle] = handle;
                                }
                            };

                        // We are in the interaction thread now. Check if a new token has been
                        // already issued or if the user-interaction dialog has been opened.
                        if (ctx->ptr->d->storedRequests.isEmpty()
                            && !ctx->reissuedToken->hasValue())
                        {
                            // That's the first failed request. A new token should be issued.

                            // Store the func.
                            ctx->ptr->d->storedRequests << retry;

                            // Request a new token. Note that this function starts a new Event Loop
                            // when it shows a modal dialog, and therefore storedRequests container
                            // could be modified inside.
                            auto token = ctx->helper->refreshToken();

                            // Set the reissued token value for previously sent requests. Currently
                            // the token value is accessed only from the interaction thread, so
                            // there is no need for additional synchronization. If it changes some
                            // day, we could switch to std::promise in Private structure to set the
                            // value and std::shared_future in lambda closures to check or read it.
                            ctx->reissuedToken->setValue(token);

                            {
                                NX_MUTEX_LOCKER lock(&ctx->ptr->d->mutex);

                                // Reinitialize the "promise". All requests sent after that will
                                // be able to request a new token (show a dialog again) on failure.
                                ctx->ptr->d->reissuedToken.reset(new Private::ReissuedToken());

                                if (token)
                                {
                                    // Update credentials for future use.
                                    ctx->ptr->d->directConnect->credentials.authToken = *token;
                                }
                            }

                            // Execute all stored requests.
                            while (!ctx->ptr->d->storedRequests.isEmpty())
                            {
                                auto retry = ctx->ptr->d->storedRequests.takeFirst();
                                retry(token);
                            }
                        }
                        else if (ctx->reissuedToken->hasValue())
                        {
                            // Token has been updated already (or the dialog was closed by User).
                            retry(ctx->reissuedToken->value());
                        }
                        else
                        {
                            // User-interaction dialog is active. Just store the func.
                            ctx->ptr->d->storedRequests << retry;
                        }
                    });
            }
            else
            {
                // Default path -- pass the result to the original callback.
                if (ctx->callback)
                    ctx->callback(success, handle, result...);
            }
        };
}

template<typename ResultType>
Callback<ResultType> ServerConnection::makeSessionAwareCallback(
    nx::vms::common::SessionTokenHelperPtr helper,
    nx::network::http::ClientPool::Request request,
    Callback<ResultType> callback,
    std::optional<nx::network::http::AsyncClient::Timeouts> timeouts)
{
    return makeSessionAwareCallbackInternal<ResultType, ResultType>(
        helper,
        std::move(request),
        std::move(callback),
        std::move(timeouts));
}

ServerConnectionBase::Result<QByteArray>::type ServerConnection::makeSessionAwareCallback(
    nx::vms::common::SessionTokenHelperPtr helper,
    nx::network::http::ClientPool::Request request,
    ServerConnectionBase::Result<QByteArray>::type callback,
    nx::network::http::AsyncClient::Timeouts timeouts)
{
    return makeSessionAwareCallbackInternal<
        QByteArray, QByteArray, const nx::network::http::HttpHeaders&>(
            helper,
            std::move(request),
            std::move(callback),
            std::move(timeouts));
}

template <typename ResultType>
Handle ServerConnection::executeRequest(
    const nx::network::http::ClientPool::Request& request,
    Callback<ResultType> callback,
    QThread* targetThread,
    std::optional<Timeouts> timeouts)
{
    if (callback)
    {
        if constexpr (std::is_base_of_v<nx::network::rest::Result, ResultType>)
        {
            NX_ASSERT(!request.url.path().startsWith("/rest/"),
                "/rest handler responses with Result if request is failed, use ErrorOrData");
        }
        const QString serverId = d->serverId.toString();
        return sendRequest(
            request,
            // Guarded function is used as in some cases callback could be executed when
            // ServerConnection instance is already destroyed.
            utils::guarded(
                this,
                [this, callback = std::move(callback), serverId](ContextPtr context)
                {
                    NX_VERBOSE(d->logTag, "<%1> Got serialized reply. OS error: %2, HTTP status: %3",
                        context->handle, context->systemError, context->getStatusCode());
                    bool success = false;
                    const auto format
                        = Qn::serializationFormatFromHttpContentType(context->response.contentType);

                    // All parsing functions can handle incorrect format.
                    auto resultPtr = std::make_shared<ResultType>(
                        parseMessageBody<ResultType>(
                            format,
                            context->response.messageBody,
                            context->getStatusCode(),
                            &success));

                    if (!success)
                        NX_VERBOSE(d->logTag, "<%1> Could not parse message body.", context->handle);

                    if (context->systemError != SystemError::noError
                        || context->getStatusCode() != nx::network::http::StatusCode::ok)
                    {
                        success = false;
                    }

                    const auto id = context->handle;

                    auto internal_callback =
                        [callback = std::move(callback), success, id, resultPtr]()
                        {
                            if (callback)
                                callback(success, id, std::move(*resultPtr));
                        };

                    invoke(context, std::move(internal_callback), success, serverId);
                }),
            targetThread,
            timeouts);
    }

    return sendRequest(request, {}, targetThread);
}

template<typename ResultType>
Handle ServerConnection::executeRequest(
    const nx::network::http::ClientPool::Request &request,
    Callback<ResultType> callback,
    nx::utils::AsyncHandlerExecutor executor,
    std::optional<Timeouts> timeouts)
{
    if (!callback)
        return sendRequest(request, {}, executor);

    if constexpr (std::is_base_of_v<nx::network::rest::Result, ResultType>)
    {
        NX_ASSERT(!request.url.path().startsWith("/rest/"),
            "/rest handler responses with Result if request is failed, use ErrorOrData");
    }
    const QString serverId = d->serverId.toString();
    return sendRequest(
        request,
        // Guarded function is used as in some cases callback could be executed when
        // ServerConnection instance is already destroyed.
        utils::guarded(
            this,
            [this, callback = std::move(callback), serverId](ContextPtr context)
            {
                NX_VERBOSE(d->logTag,
                    "<%1> Got serialized reply. OS error: %2, HTTP status: %3",
                    context->handle,
                    context->systemError,
                    context->getStatusCode());
                bool success = false;
                const auto format =
                    Qn::serializationFormatFromHttpContentType(context->response.contentType);

                // All parsing functions can handle incorrect format.
                auto resultPtr = std::make_shared<ResultType>(parseMessageBody<ResultType>(
                    format, context->response.messageBody, context->getStatusCode(), &success));

                if (!success)
                    NX_VERBOSE(d->logTag, "<%1> Could not parse message body.", context->handle);

                if (context->systemError != SystemError::noError
                    || context->getStatusCode() != nx::network::http::StatusCode::ok)
                {
                    success = false;
                }

                const auto id = context->handle;

                auto internal_callback = [callback = std::move(callback), success, id, resultPtr]()
                {
                    if (callback)
                        callback(success, id, std::move(*resultPtr));
                };

                invoke(context, std::move(internal_callback), success, serverId);
            }),
        executor,
        timeouts);
}

// This is a specialization for request with QByteArray in response. Its callback is a bit different
// from regular Result<SomeType>::type. Result<QByteArray>::type has 4 arguments:
// `(bool success, Handle requestId, QByteArray result, nx::network::http::HttpHeaders& headers)`
Handle ServerConnection::executeRequest(
    const nx::network::http::ClientPool::Request& request,
    Result<QByteArray>::type callback,
    QThread* targetThread,
    std::optional<Timeouts> timeouts)
{
    if (callback)
    {
        const QString serverId = d->serverId.toString();
        QPointer<QThread> targetThreadGuard(targetThread);
        return sendRequest(
            request,
            // Guarded function is used as in some cases callback could be executed when
            // ServerConnection instance is already destroyed.
            utils::guarded(
                this,
                [this, callback,  targetThreadGuard, serverId](ContextPtr context)
                {
                    const auto statusCode = context->getStatusCode();
                    const auto osErrorCode = context->systemError;
                    const auto id = context->handle;

                    NX_VERBOSE(
                        d->logTag,
                        "<%1> Got %2 byte(s) reply of content type %3. OS error: %4, HTTP status: %5",
                        id,
                        context->response.messageBody.size(),
                        QString::fromLatin1(context->response.contentType),
                        osErrorCode,
                        statusCode);

                    const bool success = context->hasSuccessfulResponse();
                    auto internal_callback =
                        [callback, success, id, context]()
                        {
                            callback(
                                success,
                                id,
                                context->response.messageBody,
                                context->response.headers);
                        };

                    invoke(context, internal_callback, success, serverId);
                }),
            targetThread,
            timeouts);
    }

    return sendRequest(request, {}, targetThread, timeouts);
}

Handle ServerConnection::executeRequest(
    const nx::network::http::ClientPool::Request& request,
    Callback<EmptyResponseType> callback,
    QThread* targetThread,
    std::optional<Timeouts> timeouts)
{
    if (callback)
    {
        const QString serverId = d->serverId.toString();
        return sendRequest(
            request,
            // Guarded function is used as in some cases callback could be executed when
            // ServerConnection instance is already destroyed.
            utils::guarded(
                this,
                [this, callback, serverId](ContextPtr context)
                {
                    const auto statusCode = context->getStatusCode();
                    const auto osErrorCode = context->systemError;
                    const auto id = context->handle;
                    bool success = (osErrorCode == SystemError::noError
                        && statusCode >= nx::network::http::StatusCode::ok
                        && statusCode <= nx::network::http::StatusCode::partialContent);

                    NX_VERBOSE(d->logTag, "<%1> Got reply. OS error: %2, HTTP status: %3",
                        id, osErrorCode, statusCode);

                    auto internal_callback = [callback, success, id]()
                        {
                            callback(success, id, EmptyResponseType());
                        };

                    invoke(context, internal_callback, success, serverId);
                }),
            targetThread,
            timeouts);
    }

    return sendRequest(request, {}, targetThread, timeouts);
}

void ServerConnection::cancelRequest(const Handle& requestId)
{
    std::optional<Handle> actualId;
    {
        // Check if we had re-send this request with updated credentials.
        NX_MUTEX_LOCKER lock(&d->mutex);

        if (auto it = d->substitutions.find(requestId);
            it != d->substitutions.end())
        {
            actualId = it->second;
        }
    }

    if (actualId)
    {
        NX_VERBOSE(d->logTag,
            "<%1> Cancelling request (which is actually <%2>)...", requestId, *actualId);
        d->httpClientPool->terminate(*actualId);
    }
    else
    {
        NX_VERBOSE(d->logTag, "<%1> Cancelling request...", requestId);
        d->httpClientPool->terminate(requestId);
    }
}

nx::network::http::Credentials getRequestCredentials(
    std::shared_ptr<ec2::AbstractECConnection> connection,
    const QnMediaServerResourcePtr& targetServer)
{
    using namespace nx::vms::api;
    const auto localPeerType = qnStaticCommon->localPeerType();
    if (PeerData::isClient(localPeerType))
        return connection->credentials();

    NX_ASSERT(PeerData::isServer(localPeerType), "Unexpected peer type");
    return nx::network::http::Credentials(
        targetServer->getId().toStdString(),
        nx::network::http::PasswordAuthToken(targetServer->getAuthKey().toStdString()));
}

bool setupAuth(
    const nx::vms::common::SystemContext* systemContext,
    const QnUuid& serverId,
    nx::network::http::ClientPool::Request& request,
    const QUrl& url)
{
    if (!NX_ASSERT(systemContext))
        return false;

    auto resPool = systemContext->resourcePool();
    const auto server = resPool->getResourceById<QnMediaServerResource>(serverId);
    if (!server)
        return false;

    request.url = server->getApiUrl();
    request.url.setPath(url.path());
    request.url.setQuery(url.query());

    // This header is used by the server to identify the client login session for audit.
    request.headers.emplace(
        Qn::EC2_RUNTIME_GUID_HEADER_NAME, systemContext->sessionId().toByteArray());

    const QnRoute route = QnRouter::routeTo(server);

    if (route.reverseConnect)
    {
        if (nx::vms::api::PeerData::isClient(qnStaticCommon->localPeerType()))
        {
            const auto connection = systemContext->messageBusConnection();
            if (!NX_ASSERT(connection))
                return false;

            const auto address = connection->address();
            request.url.setHost(address.address.toString());
            if ((int16_t) address.port != -1)
                request.url.setPort(address.port);
        }
        else //< Server-side option.
        {
            request.url.setHost("127.0.0.1");
            auto currentServer = systemContext->resourcePool()
                ->getResourceById<QnMediaServerResource>(systemContext->peerId());
            if (NX_ASSERT(currentServer))
            {
                const auto url = nx::utils::Url(currentServer->getUrl());
                if (url.port() > 0)
                    request.url.setPort(url.port());
            }
        }
    }
    else if (!route.addr.isNull())
    {
        request.url.setHost(route.addr.address.toString());
        request.url.setPort(route.addr.port);
    }

    // TODO: #sivanov Only client-side connection is actually used.
    const auto connection = systemContext->messageBusConnection();
    if (!connection)
        return false;

    request.headers.emplace(Qn::SERVER_GUID_HEADER_NAME, server->getId().toByteArray());
    request.credentials = getRequestCredentials(connection, server);
    request.headers.emplace(Qn::CUSTOM_USERNAME_HEADER_NAME,
        QString::fromStdString(request.credentials->username).toLower().toUtf8());
    if (!route.gatewayId.isNull())
        request.gatewayId = route.gatewayId;

    return true;
}

void setupAuthDirect(
    nx::network::http::ClientPool::Request& request,
    const QnUuid& auditId,
    nx::network::SocketAddress address,
    nx::network::http::Credentials credentials,
    QString path,
    QString query)
{
    request.url = nx::network::url::Builder()
        .setScheme(nx::network::http::kSecureUrlSchemeName)
        .setEndpoint(address)
        .setPath(path)
        .setQuery(query)
        .toUrl();

    request.credentials = std::move(credentials);

    // This header is used by the server to identify the client login session for audit.
    request.headers.emplace(Qn::EC2_RUNTIME_GUID_HEADER_NAME, auditId.toByteArray());

    // This header was used to migrate digest in the old server's db. Most probably is not needed.
    request.headers.emplace(Qn::CUSTOM_USERNAME_HEADER_NAME,
        QString::fromStdString(request.credentials->username).toLower().toUtf8());
}

nx::network::http::ClientPool::Request ServerConnection::prepareRequest(
    nx::network::http::Method method,
    const QUrl& url,
    const nx::String& contentType,
    const nx::String& messageBody)
{
    nx::network::http::ClientPool::Request request;

    bool isDirect = false, authIsSet = false;

    {
        NX_MUTEX_LOCKER lock(&d->mutex);

        if (d->directConnect)
        {
            setupAuthDirect(
                request,
                d->directConnect->sessionId,
                d->directConnect->address,
                d->directConnect->credentials,
                url.path(),
                url.query());
            isDirect = authIsSet = true;
        }
    }

    if (!isDirect)
        authIsSet = setupAuth(d->systemContext, d->serverId, request, url);

    if (!authIsSet)
        return nx::network::http::ClientPool::Request();

    request.method = method;
    request.contentType = contentType;
    request.messageBody = messageBody;
    request.headers.emplace(nx::network::http::header::kAcceptLanguage,
        nx::vms::common::appContext()->locale().toStdString());
    return request;
}

nx::network::http::ClientPool::Request ServerConnection::prepareRestRequest(
    nx::network::http::Method method,
    const QUrl& url,
    const nx::String& messageBody)
{
    static const nx::String contentType = nx::network::http::header::ContentType::kJson.toString();

    auto request = prepareRequest(method, url, contentType, messageBody);
    request.headers.emplace(nx::network::http::header::kAccept, contentType);
    request.headers.emplace(nx::network::http::header::kUserAgent, prepareUserAgent());

    return request;
}

Handle ServerConnection::sendRequest(
    const nx::network::http::ClientPool::Request& request,
    std::function<void (ContextPtr)> callback,
    QThread* thread,
    std::optional<Timeouts> timeouts)
{
    auto certificateVerifier = d->directConnect
        ? d->directConnect->certificateVerifier
        : d->systemContext->certificateVerifier();
    if (!NX_ASSERT(certificateVerifier))
        return 0;

    ContextPtr context(new nx::network::http::ClientPool::Context(
        d->serverId,
        certificateVerifier->makeAdapterFunc(
            request.gatewayId.value_or(d->serverId), request.url)));
    context->request = request;
    context->completionFunc = callback;
    context->timeouts = timeouts;
    context->setTargetThread(thread);

    return sendRequest(context);
}

Handle ServerConnection::sendRequest(
    const nx::network::http::ClientPool::Request &request,
    std::function<void (ContextPtr)> callback,
    nx::utils::AsyncHandlerExecutor executor,
    std::optional<Timeouts> timeouts)
{
    auto certificateVerifier = d->directConnect
        ? d->directConnect->certificateVerifier
        : d->systemContext->certificateVerifier();
    if (!NX_ASSERT(certificateVerifier))
        return 0;

    ContextPtr context(new nx::network::http::ClientPool::Context(
        d->serverId,
        certificateVerifier->makeAdapterFunc(
            request.gatewayId.value_or(d->serverId), request.url)));
    context->request = request;
    context->completionFunc = executor.bind(callback);
    context->timeouts = timeouts;
    context->setTargetThread(this->thread());

    return sendRequest(context);
}

Handle ServerConnection::sendRequest(const ContextPtr& context)
{
    auto metrics = nx::vms::common::appContext()->metrics();
    metrics->totalServerRequests()++;
    NX_VERBOSE(
        d->logTag, "%1: %2", metrics->totalServerRequests.name(), metrics->totalServerRequests());
    Handle requestId = d->httpClientPool->sendRequest(context);

    // Request can be complete just inside `sendRequest`, so requestId is already invalid.
    if (!requestId || context->isFinished())
        return 0;

    return requestId;
}

static ServerConnection::DebugFlags localDebugFlags = ServerConnection::DebugFlag::none;

ServerConnection::DebugFlags ServerConnection::debugFlags()
{
    return localDebugFlags;
}

void ServerConnection::setDebugFlag(DebugFlag flag, bool on)
{
    localDebugFlags.setFlag(flag, on);
}

} // namespace rest
