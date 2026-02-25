// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "server_rest_connection.h"

#include <QtCore/QPointer>
#include <QtNetwork/QAuthenticator>

#include <api/helpers/chunks_request_data.h>
#include <api/helpers/empty_request_data.h>
#include <api/helpers/send_statistics_request_data.h>
#include <api/helpers/thumbnail_request_data.h>
#include <api/model/cloud_credentials_data.h>
#include <api/model/update_information_reply.h>
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
#include <nx/metric/application_metrics_storage.h>
#include <nx/network/http/custom_headers.h>
#include <nx/network/http/http_types.h>
#include <nx/network/rest/result.h>
#include <nx/network/ssl/helpers.h>
#include <nx/network/url/url_builder.h>
#include <nx/reflect/json.h>
#include <nx/reflect/string_conversion.h>
#include <nx/reflect/urlencoded/serializer.h>
#include <nx/utils/buffer.h>
#include <nx/utils/coro/task_utils.h>
#include <nx/utils/guarded_callback.h>
#include <nx/utils/i18n/translation_manager.h>
#include <nx/utils/json/qjson.h>
#include <nx/utils/json/qt_containers_reflect.h>
#include <nx/utils/log/log.h>
#include <nx/utils/random.h>
#include <nx/vms/api/analytics/device_agent_active_setting_changed_request.h>
#include <nx/vms/api/analytics/device_agent_settings_request.h>
#include <nx/vms/api/data/device_actions.h>
#include <nx/vms/api/data/ldap.h>
#include <nx/vms/api/data/login.h>
#include <nx/vms/api/data/lookup_list_data.h>
#include <nx/vms/api/data/peer_data.h>
#include <nx/vms/api/data/site_information.h>
#include <nx/vms/api/data/site_setup.h>
#include <nx/vms/api/data/storage_encryption_data.h>
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

#include "parsing_utils.h"
#include "request_rerunner.h"

using namespace nx;
using namespace nx::coro;

namespace {

constexpr auto kJsonRpcPath = "/jsonrpc";

void proxyRequestUsingServer(
    nx::network::http::ClientPool::Request& request,
    const nx::Uuid& proxyServerId)
{
    nx::network::http::HttpHeader header(Qn::SERVER_GUID_HEADER_NAME, proxyServerId.toSimpleStdString());
    nx::network::http::insertOrReplaceHeader(&request.headers, header);
}

template<typename T>
rest::Callback<nx::network::rest::JsonResult> extractJsonResult(
    rest::Callback<T> callback)
{
    return
        [callback = std::move(callback)](bool success, rest::Handle requestId,
            const nx::network::rest::JsonResult& result)
        {
            callback(success, requestId, result.deserialized<T>());
        };
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

nx::log::Tag makeLogTag(
    rest::ServerConnection* instance, const nx::Uuid& serverId, const char *subTag)
{
    return nx::log::Tag(NX_FMT("%1 %2 [%3]",
        nx::toString(instance), subTag, serverId.toSimpleString()));
}

} // namespace

// --------------------------- public methods -------------------------------------------

namespace rest {

using HandleCallback = nx::MoveOnlyFunc<void(rest::Handle)>;

struct ServerConnection::Private
{
    ~Private()
    {
        decltype(pendingRequests) handles;
        {
            NX_MUTEX_LOCKER lock(&mutex);
            std::swap(handles, pendingRequests);
        }
        for (const auto handle: handles)
            httpClientPool->terminate(handle);
    }

    auto executeRequestAwaitable(
        nx::network::http::ClientPool::Request request,
        std::optional<nx::network::http::AsyncClient::Timeouts> timeouts,
        HandleCallback onSuspend,
        const nx::log::Tag& logTag)
    {
        struct RequestAwaiter
        {
            RequestAwaiter(
                rest::ServerConnection* connection,
                nx::network::http::ClientPool::Request request,
                std::optional<nx::network::http::AsyncClient::Timeouts> timeouts,
                HandleCallback onSuspend,
                const nx::log::Tag& logTag)
                :
                m_connection(connection),
                m_request(std::move(request)),
                m_timeouts(timeouts),
                m_onSuspend(std::move(onSuspend)),
                m_logTag(logTag)
            {
            }

            bool await_ready() const { return false; }

            void await_suspend(std::coroutine_handle<> h)
            {
                auto guard = nx::utils::ScopeGuard(
                    [this, h]()
                    {
                        m_context.reset();
                        h();
                    });

                auto connection = m_connection;

                auto removePending = nx::utils::AsyncHandlerExecutor(connection).bind(
                    [connection](Handle handle)
                    {
                        connection->d->removePending(handle);
                    });

                auto onSuspend = std::move(m_onSuspend);

                auto context = connection->prepareContext(
                    m_request,
                    [this, h = std::move(h), guard = std::move(guard),
                        removePending = std::move(removePending)](
                        nx::network::http::ClientPool::ContextPtr context) mutable
                    {
                        // Executes in asio thread.
                        const auto osErrorCode = context->systemError;

                        NX_VERBOSE(m_logTag, "<%1> Reply %2 %3 Error: %4, Status: %5",
                            context->handle, context->request.method, context->request.url,
                            osErrorCode, context->getStatusLine().statusCode);

                        m_context = std::move(context);
                        guard.disarm();
                        removePending(m_context->handle);

                        h();
                    },
                    m_timeouts);

                auto handle = connection->sendRequest(context);
                // If we resumed from sendRequest() immediately, `this` is already destroyed!
                if (handle != 0)
                {
                    connection->d->addPending(handle);
                    onSuspend(handle);
                }
            }

            nx::network::http::ClientPool::ContextPtr await_resume() const
            {
                if (!m_context)
                    throw TaskCancelException();
                return m_context;
            }

            QPointer<rest::ServerConnection> m_connection;
            nx::network::http::ClientPool::Request m_request;
            std::optional<nx::network::http::AsyncClient::Timeouts> m_timeouts;
            HandleCallback m_onSuspend;
            nx::log::Tag m_logTag;

            nx::network::http::ClientPool::ContextPtr m_context;
        };

        return RequestAwaiter{
            this->q, std::move(request), std::move(timeouts), std::move(onSuspend), logTag};
    }

    Handle executeRequest(
        nx::vms::common::SessionTokenHelperPtr helper,
        const nx::network::http::ClientPool::Request& request,
        std::unique_ptr<BaseResultContext> requestContext,
        RequestCallback callback,
        nx::utils::AsyncHandlerExecutor executor,
        std::optional<Timeouts> timeouts = std::nullopt);

    // Coroutine-based request execution, resumes the coroutine on ASIO thread!
    // Reports rest::Handle from HttpClientPool via onSuspend callback.
    nx::coro::Task<
        std::tuple<
            std::unique_ptr<BaseResultContext>,
            rest::Handle>>
    executeRequestAsync(
        nx::network::http::ClientPool::Request request,
        std::unique_ptr<BaseResultContext> requestContext,
        std::optional<Timeouts> timeouts,
        nx::vms::common::SessionTokenHelperPtr helper,
        HandleCallback onSuspend);

    void addPending(rest::Handle handle)
    {
        NX_MUTEX_LOCKER lock(&mutex);
        pendingRequests.insert(handle);
    }

    void removePending(rest::Handle handle)
    {
        NX_MUTEX_LOCKER lock(&mutex);
        pendingRequests.erase(handle);
    }

    ServerConnection* const q;
    const nx::vms::common::SystemContext* systemContext = nullptr;
    nx::network::http::ClientPool* const httpClientPool;
    const nx::Uuid auditId;
    const nx::Uuid serverId;
    const nx::log::Tag logTag;

    nx::vms::common::RequestRerunner rerunner;

    /**
     * Unique certificate func id to avoid reusing old functions when the Server Connection is
     * re-created (thus correct certificate verifier will always be used).
     */
    const nx::Uuid certificateFuncId = nx::Uuid::createUuid();

    // While most fields of this struct never change during struct's lifetime, some data can be
    // rarely updated. Therefore the following non-const fields should be protected by mutex.
    nx::Mutex mutex;

    struct DirectConnect
    {
        QPointer<nx::vms::common::AbstractCertificateVerifier> certificateVerifier;
        nx::network::SocketAddress address;
        nx::network::http::Credentials credentials;
    };
    nx::Uuid userId;
    std::optional<DirectConnect> directConnect;

    std::map<Handle, Handle> substitutions;
    std::set<Handle> pendingRequests;
};

ServerConnection::ServerConnection(
    nx::vms::common::SystemContext* systemContext,
    const nx::Uuid& serverId)
    :
    d(new Private{
        .q = this,
        .systemContext = systemContext,
        .httpClientPool = systemContext->httpClientPool(),
        .auditId = systemContext->auditId(),
        .serverId = serverId,
        .logTag = makeLogTag(this, serverId, "C")})
{
    // TODO: #sivanov Raw pointer is unsafe here as ServerConnection instance may be not deleted
    // after it's owning server (and context) are destroyed. Need to change
    // QnMediaServerResource::restConnection() method to return weak pointer instead.

    NX_VERBOSE(d->logTag, "Create");
}

ServerConnection::ServerConnection(
    nx::network::http::ClientPool* httpClientPool,
    const nx::Uuid& serverId,
    const nx::Uuid& auditId,
    AbstractCertificateVerifier* certificateVerifier,
    nx::network::SocketAddress address,
    nx::network::http::Credentials credentials)
    :
    QObject(),
    d(new Private{
        .q = this,
        .systemContext = nullptr,
        .httpClientPool = httpClientPool,
        .auditId = auditId,
        .serverId = serverId,
        .logTag = makeLogTag(this, serverId, "D"),
        .directConnect{Private::DirectConnect{
            .certificateVerifier = certificateVerifier,
            .address = std::move(address),
            .credentials = std::move(credentials)}}})
{
    NX_VERBOSE(d->logTag, "Create");

    if (NX_ASSERT(certificateVerifier))
    {
        connect(certificateVerifier, &QObject::destroyed, this,
            [this]() {
                // Skip assertion here to assert when the connection is destroyed.
                NX_WARNING(this, "Premature certificate verifier destruction");
            });
    }
}

ServerConnection::~ServerConnection()
{
    NX_VERBOSE(d->logTag, "Destroy");

    if (d->directConnect)
    {
        NX_ASSERT(d->directConnect->certificateVerifier,
            "%1: Certificate verifier already destroyed", d->logTag);
    }
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

void ServerConnection::setUserId(const nx::Uuid& id)
{
    NX_MUTEX_LOCKER lock(&d->mutex);
    d->userId = id;
}

Handle ServerConnection::cameraHistoryAsync(
    const QnChunksRequestData& request,
    Callback<nx::vms::api::CameraHistoryDataList> callback,
    nx::utils::AsyncHandlerExecutor executor)
{
    return executeGet("/ec2/cameraHistory", request.toParams(), std::move(callback), executor);
}

Handle ServerConnection::backupPositionAsyncV4(const nx::Uuid& serverId,
    const nx::Uuid& deviceId,
    Callback<nx::vms::api::BackupPositionExV4> callback,
    nx::utils::AsyncHandlerExecutor executor)
{
    const auto requestStr =
        NX_FMT("/rest/v4/servers/%1/backupPositions/%2").args(serverId, deviceId);
    return executeGet(requestStr, nx::network::rest::Params(), std::move(callback), executor);
}

Handle ServerConnection::setBackupPositionAsyncV4(const nx::Uuid& serverId,
    const nx::Uuid& deviceId,
    const nx::vms::api::BackupPositionV4& backupPosition,
    Callback<nx::vms::api::BackupPositionV4> callback,
    nx::utils::AsyncHandlerExecutor executor)
{
    const auto requestStr =
        NX_FMT("/rest/v4/servers/%1/backupPositions/%2").args(serverId, deviceId);
    return executePut(
        requestStr,
        nx::network::rest::Params(),
        "application/json",
        QJson::serialized(backupPosition),
        std::move(callback),
        executor);
}

Handle ServerConnection::setBackupPositionsAsyncV4(const nx::Uuid& serverId,
    const nx::vms::api::BackupPositionV4& backupPosition,
    Callback<nx::vms::api::BackupPositionV4> callback,
    nx::utils::AsyncHandlerExecutor executor)
{
    const auto requestStr = NX_FMT("/rest/v4/servers/%1/backupPositions", serverId);
    return executePut(
        requestStr,
        nx::network::rest::Params(),
        "application/json",
        QJson::serialized(backupPosition),
        std::move(callback),
        executor);
}

Handle ServerConnection::getServerLocalTime(
    const nx::Uuid& serverId,
    Callback<nx::network::rest::JsonResult> callback,
    nx::utils::AsyncHandlerExecutor executor)
{
    nx::network::rest::Params params{{"local", QnLexical::serialized(true)}};
    return executeGet("/api/gettime", params, std::move(callback), executor, serverId);
}

rest::Handle ServerConnection::cameraThumbnailAsync(const nx::api::CameraImageRequest& request,
    DataCallback callback,
    nx::utils::AsyncHandlerExecutor executor)
{
    if (debugFlags().testFlag(DebugFlag::disableThumbnailRequests))
        return {};

    QnThumbnailRequestData data{request, QnThumbnailRequestData::RequestType::cameraThumbnail};
    data.format = Qn::SerializationFormat::ubjson;

    return d->executeRequest(
        /*helper*/ nullptr,
        prepareRequest(
            nx::network::http::Method::get,
            prepareUrl("/ec2/cameraThumbnail", data.toParams())),
        std::make_unique<RawResultContext>(),
        RawResultContext::wrapCallback(std::move(callback)),
        executor);
}

Handle ServerConnection::sendStatisticsUsingServer(
    const nx::Uuid& proxyServerId,
    const QnSendStatisticsRequestData& statisticsData,
    PostCallback callback,
    nx::utils::AsyncHandlerExecutor executor)
{
    static const QString kPath = "/ec2/statistics/send";

    using namespace nx::network::http;
    ClientPool::Request request = prepareRequest(
        Method::post,
        prepareUrl(kPath, statisticsData.toParams()),
        header::ContentType::kJson.toString(),
        QJson::serialized(statisticsData.metricsList));
    proxyRequestUsingServer(request, proxyServerId);

    using Context = ResultContext<EmptyResponseType>;

    return d->executeRequest(
        /*helper*/ nullptr,
        request,
        std::make_unique<Context>(),
        Context::wrapCallback(std::move(callback)),
        executor);
}

Handle ServerConnection::getModuleInformation(
    Callback<ResultWithData<nx::vms::api::ModuleInformation>> callback,
    nx::utils::AsyncHandlerExecutor executor)
{
    nx::network::rest::Params params;
    return executeGet("/api/moduleInformation", params, std::move(callback), executor);
}

Handle ServerConnection::getModuleInformationAll(
    Callback<ResultWithData<QList<nx::vms::api::ModuleInformation>>> callback,
    nx::utils::AsyncHandlerExecutor executor)
{
    nx::network::rest::Params params;
    params.insert("allModules", lit("true"));
    return executeGet("/api/moduleInformation", params, std::move(callback), executor);
}

Handle ServerConnection::getServersInfo(
    bool onlyFreshInfo,
    Callback<ErrorOrData<nx::vms::api::ServerInformationV1List>>&& callback,
    nx::utils::AsyncHandlerExecutor executor)
{
    return executeGet(
        "/rest/v1/servers/*/info",
        {{"onlyFreshInfo", QnLexical::serialized(onlyFreshInfo)}},
        std::move(callback), executor);
}

Handle ServerConnection::bindSystemToCloud(
    const QString& cloudSystemId,
    const QString& cloudAuthKey,
    const QString& cloudAccountName,
    const QString& organizationId,
    const std::string& ownerSessionToken,
    Callback<ErrorOrEmpty> callback,
    nx::utils::AsyncHandlerExecutor executor)
{
    nx::vms::api::CloudSystemAuth data;
    data.systemId = cloudSystemId;
    data.authKey = cloudAuthKey;
    data.owner = cloudAccountName;
    data.organizationId = organizationId;

    auto request = prepareRestRequest(
        nx::network::http::Method::post,
        prepareUrl("/rest/v3/system/cloud/bind", /*params*/ {}),
        nx::reflect::json::serialize(data));
    request.credentials = nx::network::http::BearerAuthToken(ownerSessionToken);

    using Context = ResultContext<ErrorOrEmpty>;

    return d->executeRequest(
        /*helper*/ nullptr,
        request,
        std::make_unique<Context>(),
        Context::wrapCallback(std::move(callback)),
        executor);
}

Handle ServerConnection::unbindSystemFromCloud(
    nx::vms::common::SessionTokenHelperPtr tokenHelper,
    const QString& password,
    Callback<ErrorOrEmpty> callback,
    nx::utils::AsyncHandlerExecutor executor)
{
    nx::vms::api::LocalSiteAuth data;
    data.password = password;

    auto request = prepareRestRequest(
        nx::network::http::Method::post,
        prepareUrl("/rest/v3/system/cloud/unbind", /*params*/ {}),
        nx::reflect::json::serialize(data));

    using Context = ResultContext<ErrorOrEmpty>;

    return d->executeRequest(
        tokenHelper,
        request,
        std::make_unique<Context>(),
        Context::wrapCallback(std::move(callback)),
        executor);
}

Handle ServerConnection::dumpDatabase(
    nx::vms::common::SessionTokenHelperPtr tokenHelper,
    Callback<ErrorOrData<QByteArray>> callback,
    nx::utils::AsyncHandlerExecutor executor)
{
    auto request = prepareRequest(
        nx::network::http::Method::get,
        prepareUrl("/rest/v2/system/database", /*params*/ {}),
        nx::network::http::header::ContentType::kBinary.value);

    auto timeouts = nx::network::http::AsyncClient::Timeouts::defaults();
    timeouts.responseReadTimeout = std::chrono::minutes(5);
    timeouts.messageBodyReadTimeout = std::chrono::minutes(5);

    using Context = ResultContext<ErrorOrData<QByteArray>>;

    return d->executeRequest(
        tokenHelper,
        request,
        std::make_unique<Context>(),
        Context::wrapCallback(std::move(callback)),
        executor,
        timeouts);
}

Handle ServerConnection::restoreDatabase(
    nx::vms::common::SessionTokenHelperPtr tokenHelper,
    const QByteArray& data,
    Callback<ErrorOrEmpty> callback,
    nx::utils::AsyncHandlerExecutor executor)
{
    auto request = prepareRequest(
        nx::network::http::Method::post,
        prepareUrl("/rest/v2/system/database", /*params*/ {}),
        nx::network::http::header::ContentType::kBinary.value,
        data);

    auto timeouts = nx::network::http::AsyncClient::Timeouts::defaults();
    timeouts.sendTimeout = std::chrono::minutes(5);
    timeouts.responseReadTimeout = std::chrono::minutes(5);

    using Context = ResultContext<ErrorOrEmpty>;

    return d->executeRequest(
        tokenHelper,
        request,
        std::make_unique<Context>(),
        Context::wrapCallback(std::move(callback)),
        executor,
        timeouts);
}

Handle ServerConnection::putServerLogSettings(
    nx::vms::common::SessionTokenHelperPtr tokenHelper,
    const nx::Uuid& serverId,
    const nx::vms::api::ServerLogSettings& settings,
    Callback<ErrorOrEmpty> callback,
    nx::utils::AsyncHandlerExecutor executor)
{
    auto request = prepareRequest(
        nx::network::http::Method::put,
        prepareUrl(
            QString("/rest/v2/servers/%1/logSettings").arg(serverId.toSimpleString()),
            /*params*/ {}),
        Qn::serializationFormatToHttpContentType(Qn::SerializationFormat::json),
        nx::reflect::json::serialize(settings));

    using Context = ResultContext<ErrorOrEmpty>;

    return d->executeRequest(
        tokenHelper,
        request,
        std::make_unique<Context>(),
        Context::wrapCallback(std::move(callback)),
        executor);
}

Handle ServerConnection::patchSystemSettings(
    nx::vms::common::SessionTokenHelperPtr tokenHelper,
    const nx::vms::api::SaveableSystemSettings& settings,
    Callback<ErrorOrEmpty> callback,
    nx::utils::AsyncHandlerExecutor executor)
{
    auto request = prepareRequest(
        nx::network::http::Method::patch,
        prepareUrl(
            "/rest/v3/system/settings",
            /*params*/ {}),
        Qn::serializationFormatToHttpContentType(Qn::SerializationFormat::json),
        nx::reflect::json::serialize(settings));

    using Context = ResultContext<ErrorOrEmpty>;

    return d->executeRequest(
        tokenHelper,
        request,
        std::make_unique<Context>(),
        Context::wrapCallback(std::move(callback)),
        executor);
}

Handle ServerConnection::addFileDownload(
    const QString& fileName,
    qint64 size,
    const QByteArray& md5,
    const QUrl& url,
    const QString& peerPolicy,
    GetCallback callback,
    nx::utils::AsyncHandlerExecutor executor)
{
    return executePost(
        QString("/api/downloads/%1").arg(fileName),
        nx::network::rest::Params{
            {"size", QString::number(size)},
            {"md5", QString::fromUtf8(md5)},
            {"url", url.toString()},
            {"peerPolicy", peerPolicy}},
        std::move(callback),
        executor);
}

Handle ServerConnection::addCamera(
    const nx::Uuid& targetServerId,
    const nx::vms::api::DeviceModelForSearch& device,
    nx::vms::common::SessionTokenHelperPtr tokenHelper,
    Callback<ErrorOrData<nx::vms::api::DeviceModelForSearch>>&& callback,
    nx::utils::AsyncHandlerExecutor executor)
{
    auto request = prepareRequest(nx::network::http::Method::post,
        prepareUrl(QString("/rest/v4/devices"), {}),
        Qn::serializationFormatToHttpContentType(Qn::SerializationFormat::json),
        nx::reflect::json::serialize(device));

    proxyRequestUsingServer(request, targetServerId);

    using Context = ResultContext<ErrorOrData<nx::vms::api::DeviceModelForSearch>>;

    return d->executeRequest(
        tokenHelper,
        request,
        std::make_unique<Context>(),
        Context::wrapCallback(std::move(callback)),
        executor);
}

Handle ServerConnection::patchCamera(
    const nx::Uuid& targetServerId,
    const nx::vms::api::DeviceModelGeneral& device,
    nx::vms::common::SessionTokenHelperPtr tokenHelper,
    Callback<ErrorOrData<nx::vms::api::DeviceModelForSearch>>&& callback,
    nx::utils::AsyncHandlerExecutor executor)
{
    auto request = prepareRequest(nx::network::http::Method::patch,
        prepareUrl(NX_FMT("/rest/v4/devices/%1", device.id), {}),
        Qn::serializationFormatToHttpContentType(Qn::SerializationFormat::json),
        nx::reflect::json::serialize(device));

    proxyRequestUsingServer(request, targetServerId);

    using Context = ResultContext<ErrorOrData<nx::vms::api::DeviceModelForSearch>>;

    return d->executeRequest(
        tokenHelper,
        request,
        std::make_unique<Context>(),
        Context::wrapCallback(std::move(callback)),
        executor);
}

Handle ServerConnection::searchCamera(
    const nx::Uuid& targetServerId,
    const nx::vms::api::DeviceSearch& deviceSearchData,
    nx::vms::common::SessionTokenHelperPtr tokenHelper,
    Callback<ErrorOrData<nx::vms::api::DeviceSearch>>&& callback,
    nx::utils::AsyncHandlerExecutor executor)
{
    auto request = prepareRequest(nx::network::http::Method::post,
        prepareUrl(QString("/rest/v3/devices/*/searches/"), {}),
        Qn::serializationFormatToHttpContentType(Qn::SerializationFormat::json),
        nx::reflect::json::serialize(deviceSearchData));

    proxyRequestUsingServer(request, targetServerId);

    using Context = ResultContext<ErrorOrData<nx::vms::api::DeviceSearch>>;

    return d->executeRequest(
        tokenHelper,
        request,
        std::make_unique<Context>(),
        Context::wrapCallback(std::move(callback)),
        executor);
}

Handle ServerConnection::searchCameraStatus(
    const nx::Uuid& targetServerId,
    const QString& searchId,
    nx::vms::common::SessionTokenHelperPtr tokenHelper,
    Callback<ErrorOrData<nx::vms::api::DeviceSearch>>&& callback,
    nx::utils::AsyncHandlerExecutor executor)
{
    auto request = prepareRequest(nx::network::http::Method::get,
        prepareUrl(NX_FMT("/rest/v3/devices/*/searches/%1", searchId), {}));

    proxyRequestUsingServer(request, targetServerId);

    using Context = ResultContext<ErrorOrData<nx::vms::api::DeviceSearch>>;

    return d->executeRequest(
        tokenHelper,
        request,
        std::make_unique<Context>(),
        Context::wrapCallback(std::move(callback)),
        executor);
}

Handle ServerConnection::searchCameraStop(
    const nx::Uuid& targetServerId,
    const QString& searchId,
    nx::vms::common::SessionTokenHelperPtr tokenHelper,
    Callback<ErrorOrEmpty> callback,
    nx::utils::AsyncHandlerExecutor executor)
{
    auto request = prepareRequest(nx::network::http::Method::delete_,
        prepareUrl(NX_FMT("/rest/v3/devices/*/searches/%1", searchId), {}));

    proxyRequestUsingServer(request, targetServerId);

    using Context = ResultContext<ErrorOrEmpty>;

    return d->executeRequest(
        tokenHelper,
        request,
        std::make_unique<Context>(),
        Context::wrapCallback(std::move(callback)),
        executor);
}

Handle ServerConnection::executeAnalyticsAction(
    const nx::vms::api::AnalyticsAction& action,
    Callback<nx::network::rest::JsonResult> callback,
    nx::utils::AsyncHandlerExecutor executor)
{
    return executePost(
        "/api/executeAnalyticsAction",
        QJson::serialized(action),
        std::move(callback),
        executor);
}

Handle ServerConnection::getRemoteArchiveSynchronizationStatus(
    Callback<ErrorOrData<nx::vms::api::RemoteArchiveSynchronizationStatusList>>&& callback,
    nx::utils::AsyncHandlerExecutor executor)
{
    return executeGet(
        "/rest/v3/servers/this/remoteArchive/*/sync",
        nx::network::rest::Params(),
        std::move(callback),
        executor);
}

Handle ServerConnection::getOverlappedIds(
    const QString& nvrGroupId,
    Callback<nx::vms::api::OverlappedIdResponse> callback,
    nx::utils::AsyncHandlerExecutor executor)
{
    return executeGet(
        "/api/overlappedIds",
        nx::network::rest::Params{{"groupId", nvrGroupId}},
        Callback<nx::network::rest::JsonResult>(
            [callback = std::move(callback)](
                bool success, Handle requestId, const nx::network::rest::JsonResult& result)
            {
                callback(
                    success,
                    requestId,
                    result.deserialized<nx::vms::api::OverlappedIdResponse>());
            }),
        executor);
}

Handle ServerConnection::setOverlappedId(
    const QString& nvrGroupId,
    int overlappedId,
    Callback<nx::vms::api::OverlappedIdResponse> callback,
    nx::utils::AsyncHandlerExecutor executor)
{
    nx::vms::api::SetOverlappedIdRequest request;
    request.groupId = nvrGroupId;
    request.overlappedId = overlappedId;

    return executePost(
        "/api/overlappedIds",
        nx::network::rest::Params(),
        Qn::serializationFormatToHttpContentType(Qn::SerializationFormat::json),
        QJson::serialized(request),
        Callback<nx::network::rest::JsonResult>(
            [callback = std::move(callback)](
                bool success, Handle requestId, const nx::network::rest::JsonResult& result)
            {
                callback(
                    success,
                    requestId,
                    result.deserialized<nx::vms::api::OverlappedIdResponse>());
            }),
        executor);
}

Handle ServerConnection::executeEventAction(
    const nx::vms::api::EventActionData& action,
    Callback<nx::network::rest::Result> callback,
    nx::utils::AsyncHandlerExecutor executor,
    std::optional<nx::Uuid> proxyToServer)
{
    return executePost(
        "/api/executeEventAction",
        QJson::serialized(action),
        std::move(callback),
        executor,
        proxyToServer);
}

Handle ServerConnection::addFileUpload(
    const nx::Uuid& serverId,
    const QString& fileName,
    qint64 size,
    qint64 chunkSize,
    const QByteArray& md5,
    qint64 ttl,
    bool recreateIfExists,
    AddUploadCallback callback,
    nx::utils::AsyncHandlerExecutor executor)
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
        std::move(callback),
        executor,
        serverId);
}

Handle ServerConnection::removeFileDownload(
    const nx::Uuid& serverId,
    const QString& fileName,
    bool deleteData,
    PostCallback callback,
    nx::utils::AsyncHandlerExecutor executor)
{
    return executeDelete(
        lit("/api/downloads/%1").arg(fileName),
        nx::network::rest::Params{{lit("deleteData"), QnLexical::serialized(deleteData)}},
        std::move(callback),
        executor,
        serverId);
}

Handle ServerConnection::fileChunkChecksums(
    const nx::Uuid& serverId,
    const QString& fileName,
    GetCallback callback,
    nx::utils::AsyncHandlerExecutor executor)
{
    return executeGet(
        lit("/api/downloads/%1/checksums").arg(fileName),
        nx::network::rest::Params(),
        std::move(callback),
        executor,
        serverId);
}

Handle ServerConnection::downloadFileChunk(
    const nx::Uuid& serverId,
    const QString& fileName,
    int chunkIndex,
    DataCallback callback,
    nx::utils::AsyncHandlerExecutor executor)
{
    auto request = prepareRequest(
        nx::network::http::Method::get,
        prepareUrl(
            nx::format("/api/downloads/%1/chunks/%2", fileName, chunkIndex),
            /*params*/{}));

    proxyRequestUsingServer(request, serverId);

    return d->executeRequest(
        /*helper*/ nullptr,
        request,
        std::make_unique<RawResultContext>(),
        RawResultContext::wrapCallback(std::move(callback)),
        executor);
}

Handle ServerConnection::downloadFileChunkFromInternet(
    const nx::Uuid& serverId,
    const QString& fileName,
    const nx::Url& url,
    int chunkIndex,
    int chunkSize,
    qint64 fileSize,
    DataCallback callback,
    nx::utils::AsyncHandlerExecutor executor)
{
    auto request = prepareRequest(
        nx::network::http::Method::get,
        prepareUrl(
            nx::format("/api/downloads/%1/chunks/%2", fileName, chunkIndex),
            {
                {"url", url.toString()},
                {"chunkSize", QString::number(chunkSize)},
                {"fileSize", QString::number(fileSize)},
                {"fromInternet", "true"}
            }));

    proxyRequestUsingServer(request, serverId);

    return d->executeRequest(
        /*helper*/ nullptr,
        request,
        std::make_unique<RawResultContext>(),
        RawResultContext::wrapCallback(std::move(callback)),
        executor);
}

Handle ServerConnection::uploadFileChunk(
    const nx::Uuid& serverId,
    const QString& fileName,
    int index,
    const QByteArray& data,
    PostCallback callback,
    nx::utils::AsyncHandlerExecutor executor)
{
    return executePut(
        lit("/api/downloads/%1/chunks/%2").arg(fileName).arg(index),
        nx::network::rest::Params(),
        "application/octet-stream",
        data,
        std::move(callback),
        executor,
        serverId);
}

Handle ServerConnection::downloadsStatus(
    GetCallback callback,
    nx::utils::AsyncHandlerExecutor executor)
{
    return executeGet(
        lit("/api/downloads/status"),
        nx::network::rest::Params(),
        std::move(callback),
        executor);
}

Handle ServerConnection::fileDownloadStatus(
    const nx::Uuid& serverId,
    const QString& fileName,
    GetCallback callback,
    nx::utils::AsyncHandlerExecutor executor)
{
    return executeGet(
        QString("/api/downloads/%1/status").arg(fileName),
        nx::network::rest::Params(),
        std::move(callback),
        executor,
        serverId);
}

Handle ServerConnection::getTimeOfServersAsync(
    Callback<MultiServerTimeData> callback,
    nx::utils::AsyncHandlerExecutor executor)
{
    return executeGet("/ec2/getTimeOfServers", nx::network::rest::Params(), std::move(callback), executor);
}

Handle ServerConnection::addVirtualCamera(
    const nx::Uuid& serverId,
    const QString& name,
    GetCallback callback,
    nx::utils::AsyncHandlerExecutor executor)
{
    return executePost(
        "/api/virtualCamera/add",
        nx::network::rest::Params{{ "name", name }},
        std::move(callback),
        executor,
        serverId);
}

Handle ServerConnection::prepareVirtualCameraUploads(
    const QnVirtualCameraResourcePtr& camera,
    const QnVirtualCameraPrepareData& data,
    GetCallback callback,
    nx::utils::AsyncHandlerExecutor executor)
{
    return executePost(
        "/api/virtualCamera/prepare",
        nx::network::rest::Params{ { "cameraId", camera->getId().toSimpleString() } },
        nx::network::http::header::ContentType::kJson.toString(),
        QJson::serialized(data),
        std::move(callback),
        executor,
        /*timeouts*/ {},
        camera->getParentId());
}

Handle ServerConnection::virtualCameraStatus(
    const QnVirtualCameraResourcePtr& camera,
    GetCallback callback,
    nx::utils::AsyncHandlerExecutor executor)
{
    return executeGet(
        "/api/virtualCamera/status",
        nx::network::rest::Params{ { lit("cameraId"), camera->getId().toSimpleString() } },
        std::move(callback),
        executor,
        camera->getParentId());
}

Handle ServerConnection::lockVirtualCamera(
    const QnVirtualCameraResourcePtr& camera,
    const QnUserResourcePtr& user,
    qint64 ttl,
    GetCallback callback,
    nx::utils::AsyncHandlerExecutor executor)
{
    return executePost(
        "/api/virtualCamera/lock",
        nx::network::rest::Params{
            { "cameraId", camera->getId().toSimpleString() },
            { "userId", user->getId().toSimpleString() },
            { "ttl", QString::number(ttl) } },
        std::move(callback),
        executor,
        camera->getParentId());
}

Handle ServerConnection::extendVirtualCameraLock(
    const QnVirtualCameraResourcePtr& camera,
    const QnUserResourcePtr& user,
    const nx::Uuid& token,
    qint64 ttl,
    GetCallback callback,
    nx::utils::AsyncHandlerExecutor executor)
{
    return executePost(
        "/api/virtualCamera/extend",
        nx::network::rest::Params{
            { "cameraId", camera->getId().toSimpleString() },
            { "token", token.toSimpleString() },
            { "userId", user->getId().toSimpleString() },
            { "ttl", QString::number(ttl) } },
        std::move(callback),
        executor,
        camera->getParentId());
}

Handle ServerConnection::releaseVirtualCameraLock(
    const QnVirtualCameraResourcePtr& camera,
    const nx::Uuid& token,
    GetCallback callback,
    nx::utils::AsyncHandlerExecutor executor)
{
    return executePost(
        "/api/virtualCamera/release",
        nx::network::rest::Params{
            { "cameraId", camera->getId().toSimpleString() },
            { "token", token.toSimpleString() } },
        std::move(callback),
        executor,
        camera->getParentId());
}

Handle ServerConnection::consumeVirtualCameraFile(
    const QnVirtualCameraResourcePtr& camera,
    const nx::Uuid& token,
    const QString& uploadId,
    qint64 startTimeMs,
    PostCallback callback,
    nx::utils::AsyncHandlerExecutor executor)
{
    return executePost(
        "/api/virtualCamera/consume",
        nx::network::rest::Params{
            { "cameraId", camera->getId().toSimpleString() },
            { "token", token.toSimpleString() },
            { "uploadId", uploadId },
            { "startTime", QString::number(startTimeMs) } },
        std::move(callback),
        executor,
        camera->getParentId());
}

Handle ServerConnection::getStatistics(
    const nx::Uuid& serverId,
    ServerConnection::GetCallback callback,
    nx::utils::AsyncHandlerExecutor executor)
{
    return executeGet("/api/statistics", {}, std::move(callback), executor, serverId);
}

Handle ServerConnection::getAuditLogRecords(
    std::chrono::milliseconds from,
    std::chrono::milliseconds to,
    UbJsonResultCallback callback,
    nx::utils::AsyncHandlerExecutor executor,
    std::optional<nx::Uuid> proxyToServer)
{
    return getUbJsonResult(
        "/api/auditLog",
        {
            {"from", QString::number(from.count())},
            {"to", QString::number(to.count())},
        },
        std::move(callback),
        executor,
        proxyToServer);
}

Handle ServerConnection::eventLog(
    const nx::vms::api::rules::EventLogFilter& filter,
    Callback<ErrorOrData<nx::vms::api::rules::EventLogRecordList>> callback,
    nx::utils::AsyncHandlerExecutor executor,
    std::optional<Timeouts> timeouts)
{
    QJsonValue value;
    QJson::serialize(filter, &value);
    NX_ASSERT(value.isObject());
    return executeGet(
        "rest/v4/events/log",
        nx::network::rest::Params::fromJson(value.toObject()),
        std::move(callback),
        executor,
        /*proxyToServer*/{},
        timeouts);
}

Handle ServerConnection::createSoftTrigger(
    const nx::vms::api::rules::SoftTriggerData& data,
    Callback<ErrorOrEmpty> callback,
    nx::utils::AsyncHandlerExecutor executor)
{
    return executePost(
        "rest/v4/events/triggers",
        nx::reflect::json::serialize(data),
        std::move(callback),
        executor);
}

Handle ServerConnection::getEventsToAcknowledge(
    Callback<ErrorOrData<nx::vms::api::rules::EventLogRecordList>> callback,
    nx::utils::AsyncHandlerExecutor executor)
{
    return executeGet("rest/v4/events/acknowledges", {}, std::move(callback), executor);
}

Handle ServerConnection::acknowledge(
    const nx::vms::api::rules::AcknowledgeBookmark& bookmark,
    Callback<ErrorOrData<nx::vms::api::BookmarkV3>> callback,
    nx::utils::AsyncHandlerExecutor executor)
{
    if (!NX_ASSERT(!bookmark.actionServerId.isNull()))
        return {};

    return executePost(
        "rest/v4/events/acknowledges",
        nx::reflect::json::serialize(bookmark),
        std::move(callback),
        executor);
}

Handle ServerConnection::getCameraCredentials(
    const nx::Uuid& deviceId,
    Callback<QAuthenticator> callback,
    nx::utils::AsyncHandlerExecutor executor)
{
    const auto request = prepareRequest(
        nx::network::http::Method::get,
        prepareUrl(nx::format("/rest/v1/devices/%1", deviceId), {{"_with", "credentials"}}),
        Qn::serializationFormatToHttpContentType(Qn::SerializationFormat::json));

    return d->executeRequest(
        /*helper*/ nullptr,
        request,
        std::make_unique<RawResultContext>(),
        RawResultContext::wrapCallback(
            [callback = std::move(callback)](
                bool success,
                Handle requestId,
                QByteArray result,
                const nx::network::http::HttpHeaders& /*headers*/)
            {
                nx::vms::api::DeviceModelV1 resultObject;

                if (success)
                {
                    success = QJson::deserialize(result, &resultObject)
                        && resultObject.credentials.has_value()
                        && resultObject.credentials->password != nx::Url::kMaskedPassword;
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
        executor);
}

Handle ServerConnection::changeCameraPassword(
    const QnVirtualCameraResourcePtr& camera,
    const QAuthenticator& auth,
    Callback<ErrorOrEmpty> callback,
    nx::utils::AsyncHandlerExecutor executor)
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
        executor);
}

int ServerConnection::checkCameraList(
    const nx::Uuid& serverId,
    const QnVirtualCameraResourceList& cameras,
    Callback<QnCameraListReply> callback,
    nx::utils::AsyncHandlerExecutor executor)
{
    QnCameraListReply camList;
    for (const auto& c: cameras)
        camList.physicalIdList << c->getPhysicalId();

    return executePost(
        "/api/checkDiscovery",
        QJson::serialized(camList),
        extractJsonResult<QnCameraListReply>(std::move(callback)),
        executor,
        serverId);

}

Handle ServerConnection::lookupObjectTracks(
    const nx::analytics::db::Filter& request,
    bool isLocal,
    Callback<nx::analytics::db::LookupResult> callback,
    nx::utils::AsyncHandlerExecutor executor,
    std::optional<nx::Uuid> proxyToServer)
{
    nx::network::rest::Params queryParams;
    nx::analytics::db::serializeToParams(request, &queryParams);
    queryParams.insert("isLocal", isLocal? "true" : "false");

    return executeGet(
        "/ec2/analyticsLookupObjectTracks",
        queryParams,
        std::move(callback),
        executor,
        proxyToServer);
}

//--------------------------------------------------------------------------------------------------

Handle ServerConnection::getEngineAnalyticsSettings(
    const nx::vms::common::AnalyticsEngineResourcePtr& engine,
    Callback<nx::vms::api::analytics::EngineSettingsResponse>&& callback,
    nx::utils::AsyncHandlerExecutor executor)
{
    return executeGet(
        "/ec2/analyticsEngineSettings",
        nx::network::rest::Params{
            {"analyticsEngineId", engine->getId().toSimpleString()}
        },
        extractJsonResult<nx::vms::api::analytics::EngineSettingsResponse>(std::move(callback)),
        executor);
}

Handle ServerConnection::setEngineAnalyticsSettings(
    const nx::vms::common::AnalyticsEngineResourcePtr& engine,
    const QJsonObject& settings,
    Callback<nx::vms::api::analytics::EngineSettingsResponse>&& callback,
    nx::utils::AsyncHandlerExecutor executor)
{
    nx::vms::api::analytics::EngineSettingsRequest request;
    request.settingsValues = settings;
    request.analyticsEngineId = engine->getId();
    using namespace nx::vms::api::analytics;
    return executePost<nx::network::rest::JsonResult>(
        "/ec2/analyticsEngineSettings",
        QJson::serialized(request),
        extractJsonResult<EngineSettingsResponse>(std::move(callback)),
        executor);
}

Handle ServerConnection::engineAnalyticsActiveSettingsChanged(
    const nx::vms::common::AnalyticsEngineResourcePtr& engine,
    const QString& activeElement,
    const QJsonObject& settingsModel,
    const QJsonObject& settingsValues,
    const QJsonObject& paramValues,
    Callback<nx::vms::api::analytics::EngineActiveSettingChangedResponse>&& callback,
    nx::utils::AsyncHandlerExecutor executor)
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
        executor);
}

Handle ServerConnection::postMetadata(
    const std::string& integrationUserSessionToken,
    const QString& path,
    const QByteArray& messageBody,
    Callback<ErrorOrEmpty>&& callback,
    nx::utils::AsyncHandlerExecutor executor)
{
    auto request = prepareRestRequest(
        nx::network::http::Method::post,
        prepareUrl(path, /*params*/ {}),
        messageBody);

    request.credentials = nx::network::http::BearerAuthToken(integrationUserSessionToken);

    using Context = ResultContext<ErrorOrEmpty>;

    return d->executeRequest(
        /*helper*/ nullptr,
        request,
        std::make_unique<Context>(),
        Context::wrapCallback(std::move(callback)),
        executor);
}

Handle ServerConnection::getDeviceAnalyticsSettings(
    const QnVirtualCameraResourcePtr& device,
    const nx::vms::common::AnalyticsEngineResourcePtr& engine,
    Callback<nx::vms::api::analytics::DeviceAgentSettingsResponse>&& callback,
    nx::utils::AsyncHandlerExecutor executor)
{
    using namespace nx::vms::api::analytics;
    return executeGet(
        "/ec2/deviceAnalyticsSettings",
        nx::network::rest::Params{
            {"deviceId", device->getId().toSimpleString()},
            {"analyticsEngineId", engine->getId().toSimpleString()},
        },
        extractJsonResult<DeviceAgentSettingsResponse>(std::move(callback)),
        executor);
}

Handle ServerConnection::setDeviceAnalyticsSettings(
    const QnVirtualCameraResourcePtr& device,
    const nx::vms::common::AnalyticsEngineResourcePtr& engine,
    const QJsonObject& settingsValues,
    const QJsonObject& settingsModel,
    Callback<nx::vms::api::analytics::DeviceAgentSettingsResponse>&& callback,
    nx::utils::AsyncHandlerExecutor executor)
{
    nx::vms::api::analytics::DeviceAgentSettingsRequest request;
    request.settingsValues = settingsValues;
    request.settingsModel = settingsModel;
    request.analyticsEngineId = engine->getId();
    request.deviceId = device->getId().toSimpleString();

    return executePost<nx::network::rest::JsonResult>(
        "/ec2/deviceAnalyticsSettings",
        QJson::serialized(request),
        extractJsonResult<nx::vms::api::analytics::DeviceAgentSettingsResponse>(std::move(callback)),
        executor);
}

Handle ServerConnection::deviceAnalyticsActiveSettingsChanged(
    const QnVirtualCameraResourcePtr& device,
    const nx::vms::common::AnalyticsEngineResourcePtr& engine,
    const QString& activeElement,
    const QJsonObject& settingsModel,
    const QJsonObject& settingsValues,
    const QJsonObject& paramValues,
    Callback<nx::vms::api::analytics::DeviceAgentActiveSettingChangedResponse>&& callback,
    nx::utils::AsyncHandlerExecutor executor)
{
    nx::vms::api::analytics::DeviceAgentActiveSettingChangedRequest request;
    request.analyticsEngineId = engine->getId();
    request.deviceId = device->getId().toSimpleString();
    request.activeSettingName = activeElement;
    request.settingsModel = settingsModel;
    request.settingsValues = settingsValues;
    request.paramValues = paramValues;

    return executePost<nx::network::rest::JsonResult>(
        "/ec2/notifyDeviceAnalyticsActiveSettingChanged",
        QJson::serialized(request),
        extractJsonResult<nx::vms::api::analytics::DeviceAgentActiveSettingChangedResponse>(
            std::move(callback)),
        executor);
}

Handle ServerConnection::startArchiveRebuild(const nx::Uuid& serverId,
    const QString pool,
    Callback<ErrorOrData<nx::vms::api::StorageScanInfoFull>>&& callback,
    nx::utils::AsyncHandlerExecutor executor)
{
    const auto endpoint =
        NX_FMT("/rest/v2/servers/%1/rebuildArchive/%2", serverId, pool);
    return executePost(endpoint, nx::network::rest::Params(), std::move(callback), executor);
}

Handle ServerConnection::getArchiveRebuildProgress(const nx::Uuid& serverId,
    const QString pool,
    Callback<ErrorOrData<nx::vms::api::StorageScanInfoFull>>&& callback,
    nx::utils::AsyncHandlerExecutor executor)
{
    const auto endpoint =
        NX_FMT("/rest/v2/servers/%1/rebuildArchive/%2", serverId, pool);
    return executeGet(endpoint,
        nx::network::rest::Params{{"_keepDefault", QnLexical::serialized(true)}},
        std::move(callback),
        executor);
}

Handle ServerConnection::stopArchiveRebuild(const nx::Uuid& serverId,
    const QString pool,
    Callback<ErrorOrEmpty>&& callback,
    nx::utils::AsyncHandlerExecutor executor)
{
    const auto endpoint =
        NX_FMT("/rest/v2/servers/%1/rebuildArchive/%2", serverId, pool);
    return executeDelete(endpoint, nx::network::rest::Params(), std::move(callback), executor);
}

Handle ServerConnection::postJsonResult(
    const QString& action,
    const nx::network::rest::Params& params,
    const QByteArray& body,
    JsonResultCallback&& callback,
    nx::utils::AsyncHandlerExecutor executor,
    std::optional<Timeouts> timeouts,
    std::optional<nx::Uuid> proxyToServer)
{
    const auto contentType = Qn::serializationFormatToHttpContentType(Qn::SerializationFormat::json);
    return executePost<nx::network::rest::JsonResult>(
        action,
        params,
        contentType,
        body,
        std::move(callback),
        executor,
        timeouts,
        proxyToServer);
}

using JsonRpcRequestIdType = decltype(nx::vms::api::JsonRpcRequest::id);
using JsonRpcResponseIdType = decltype(nx::vms::api::JsonRpcResponse::id);

std::tuple<
    std::unordered_set<JsonRpcRequestIdType>,
    std::vector<nx::vms::api::JsonRpcResponse>>
extractJsonRpcExpired(const rest::ErrorOrData<JsonRpcResultType>& result)
{
    if (!result)
        return {};

    const auto responseArray = std::get_if<
        std::vector<nx::vms::api::JsonRpcResponse>>(&*result);

    if (!responseArray)
        return {};

    std::unordered_set<JsonRpcRequestIdType> ids;
    std::vector<nx::vms::api::JsonRpcResponse> responses;
    responses.reserve(responseArray->size());
    for (const auto& response: *responseArray)
    {
        if (isSessionExpiredError(response))
        {
            if (const auto intId = std::get_if<nx::json_rpc::NumericId>(&response.id))
                ids.insert(*intId);
            else if (const auto strId = std::get_if<nx::json_rpc::StringId>(&response.id))
                ids.insert(*strId);
        }
        responses.push_back(response.copy());
    }
    return {std::move(ids), std::move(responses)};
}

bool mergeJsonRpcResults(
    std::vector<nx::vms::api::JsonRpcResponse>& originalResponse,
    rest::ErrorOrData<JsonRpcResultType> result)
{
    if (!result)
    {
        // Server could not handle the request.
        auto error = result.error();

        // For all requests with expired session fill in error from single rest::Result.
        for (auto& response: originalResponse)
        {
            if (isSessionExpiredError(response))
            {
                response = nx::json_rpc::Response::makeError(response.id,
                    nx::json_rpc::Error::applicationError,
                    error.errorString.toStdString(),
                    error);
            }
        }
        return true;
    }

    auto responseArray = std::get_if<std::vector<nx::vms::api::JsonRpcResponse>>(&*result);
    if (!responseArray)
    {
        // This should not happen because original requests were valid. But handle it anyway.

        if (auto error = std::get_if<nx::vms::api::JsonRpcResponse>(&*result))
        {
            // For all requests with expired session fill in error from single json-rpc response.
            for (auto& response: originalResponse)
            {
                if (isSessionExpiredError(response))
                {
                    response.result = {};
                    response.error = std::move(error->error);
                }
            }
            return true;
        }
        return false;
    }

    // Build a map for faster response replacement.
    std::unordered_map<JsonRpcResponseIdType, nx::vms::api::JsonRpcResponse*> idToResponse;

    for (auto& response: *responseArray)
    {
        if (!std::holds_alternative<std::nullptr_t>(response.id))
            idToResponse.insert({response.id, &response});
    }

    for (auto& response: originalResponse)
    {
        // Replace original response with the new one if it has the same id.
        if (auto it = idToResponse.find(response.id); it != idToResponse.end())
            response = std::move(*it->second);
    }

    return true;
}

class JsonRpcResultContext: public BaseResultContext
{
public:
    JsonRpcResultContext(std::vector<nx::vms::api::JsonRpcRequest> requests):
        result(std::unexpected(nx::network::rest::Result::notImplemented())),
        requestData(std::move(requests))
    {
    }

    void parse(
        Qn::SerializationFormat format,
        const QByteArray& messageBody,
        SystemError::ErrorCode systemError,
        const nx::network::http::StatusLine& statusLine,
        const nx::network::http::HttpHeaders&) override
    {
        auto parsedResult = parseMessageBody<decltype(result)>(
            format, messageBody, statusLine, &success);

        if (!success
            || systemError != SystemError::noError
            || statusLine.statusCode != nx::network::http::StatusCode::ok)
        {
            success = false;
            result = std::move(parsedResult);
            return;
        }

        if (!expiredIds.empty() && mergeJsonRpcResults(originalResponse, std::move(parsedResult)))
        {
            // Even if the new request failed, it is still
            // considered as json-rpc success.
            success = true;
            std::vector<nx::vms::api::JsonRpcResponse> responses;
            responses.reserve(originalResponse.size());
            for (const auto& r: originalResponse)
                responses.emplace_back(r.copy());
            result = std::move(responses);
            expiredIds.clear(); //< isSessionExpired() should return false so request can proceed.
            return;
        }

        std::tie(expiredIds, originalResponse) = extractJsonRpcExpired(parsedResult);
        result = std::move(parsedResult);
    }

    virtual nx::network::http::ClientPool::Request fixup(
        const nx::network::http::ClientPool::Request& request) override
    {
        auto fixedRequest = request;

        // Update message body to resend only failed json-rpc requests.
        std::vector<nx::vms::api::JsonRpcRequest> newRequests;
        for (const auto& request: requestData)
        {
            if (expiredIds.contains(request.id))
                newRequests.emplace_back(request.copy());
        }
        fixedRequest.messageBody = nx::reflect::json::serialize(newRequests);

        return fixedRequest;
    }

    bool isSessionExpired() const override
    {
        return !expiredIds.empty();
    }

    virtual bool isSuccess() const override
    {
        return success;
    }

    rest::ErrorOrData<JsonRpcResultType>&& getResult() &&
    {
        return std::move(result);
    }

private:
    rest::ErrorOrData<JsonRpcResultType> result;

    std::unordered_set<JsonRpcRequestIdType> expiredIds;
    std::vector<nx::vms::api::JsonRpcResponse> originalResponse;
    std::vector<nx::vms::api::JsonRpcRequest> requestData;

    bool success = false;
};

Handle ServerConnection::jsonRpcBatchCall(
    nx::vms::common::SessionTokenHelperPtr tokenHelper,
    std::vector<nx::vms::api::JsonRpcRequest> requests,
    JsonRpcBatchResultCallback&& callback,
    nx::utils::AsyncHandlerExecutor executor,
    std::optional<Timeouts> timeouts)
{
    auto request = prepareRequest(
        nx::network::http::Method::post,
        prepareUrl(kJsonRpcPath, /*params*/ {}),
        Qn::serializationFormatToHttpContentType(Qn::SerializationFormat::json),
        nx::reflect::json::serialize(requests));

    auto internalCallback =
        [callback = std::move(callback)](
            bool success,
            Handle requestId,
            rest::ErrorOrData<JsonRpcResultType> result)
        {
            if (success)
            {
                if (result)
                {
                    if (auto responseArray = std::get_if<
                        std::vector<nx::vms::api::JsonRpcResponse>>(&*result))
                    {
                        callback(success, requestId, std::move(*responseArray));
                        return;
                    }
                }
                NX_ASSERT(false, "jsonrpc success but response data is invalid");
                return;
            }

            if (!result)
            {
                std::vector<nx::json_rpc::Response> response;
                response.emplace_back(nx::json_rpc::Response::makeError(std::nullptr_t{},
                    nx::json_rpc::Error::applicationError,
                    result.error().errorString.toStdString(),
                    result.error()));
                callback(success, requestId, std::move(response));
                return;
            }

            if (auto singleResponse = std::get_if<nx::vms::api::JsonRpcResponse>(&*result))
            {
                std::vector<nx::json_rpc::Response> response;
                response.emplace_back(std::move(*singleResponse));
                callback(success, requestId, std::move(response));
            }
        };

    return d->executeRequest(
        tokenHelper,
        request,
        std::make_unique<JsonRpcResultContext>(std::move(requests)),
        [callback = std::move(internalCallback)](
            std::unique_ptr<BaseResultContext> resultContext, rest::Handle handle)
        {
            auto context = static_cast<JsonRpcResultContext*>(resultContext.get());
            callback(context->isSuccess(), handle, std::move(*context).getResult());
        },
        executor,
        timeouts);
}

Handle ServerConnection::getUbJsonResult(
    const QString& path,
    nx::network::rest::Params params,
    UbJsonResultCallback&& callback,
    nx::utils::AsyncHandlerExecutor executor,
    std::optional<nx::Uuid> proxyToServer)
{
    if (!params.contains("format"))
        params.insert("format", "ubjson");
    return executeGet(path, params, std::move(callback), executor, proxyToServer);
}

Handle ServerConnection::getJsonResult(
    const QString& path,
    nx::network::rest::Params params,
    JsonResultCallback&& callback,
    nx::utils::AsyncHandlerExecutor executor,
    std::optional<nx::Uuid> proxyToServer)
{
    if (!params.contains("format"))
        params.insert("format", "json");
    return executeGet(path, params, std::move(callback), executor, proxyToServer);
}

Handle ServerConnection::getRawResult(
    const QString& path,
    const nx::network::rest::Params& params,
    DataCallback callback,
    nx::utils::AsyncHandlerExecutor executor)
{
    return d->executeRequest(
        /*helper*/ nullptr,
        prepareRequest(nx::network::http::Method::get, prepareUrl(path, params)),
        std::make_unique<RawResultContext>(),
        RawResultContext::wrapCallback(std::move(callback)),
        executor);
}

template <typename ResultType>
Handle ServerConnection::sendRequest(
    nx::vms::common::SessionTokenHelperPtr helper,
    nx::network::http::Method method,
    const QString& action,
    const nx::network::rest::Params& params,
    const nx::String& body,
    Callback<ResultType>&& callback,
    nx::utils::AsyncHandlerExecutor executor,
    const nx::network::http::HttpHeaders& customHeaders)
{
    auto request = prepareRequest(
        method,
        prepareUrl(action, params),
        Qn::serializationFormatToHttpContentType(Qn::SerializationFormat::json),
        body);

    for (const auto& header: customHeaders)
        nx::network::http::insertOrReplaceHeader(&request.headers, header);

    using Context = ResultContext<ResultType>;

    return d->executeRequest(
        helper,
        request,
        std::make_unique<Context>(),
        Context::wrapCallback(std::move(callback)),
        executor);
}

template
Handle NX_VMS_COMMON_API ServerConnection::sendRequest(
    nx::vms::common::SessionTokenHelperPtr helper,
    nx::network::http::Method method,
    const QString& action,
    const nx::network::rest::Params& params,
    const nx::String& body,
    Callback<ErrorOrEmpty>&& callback,
    nx::utils::AsyncHandlerExecutor executor,
    const nx::network::http::HttpHeaders& customHeaders);

template
Handle NX_VMS_COMMON_API ServerConnection::sendRequest(
    nx::vms::common::SessionTokenHelperPtr helper,
    nx::network::http::Method method,
    const QString& action,
    const nx::network::rest::Params& params,
    const nx::String& body,
    Callback<ErrorOrData<QByteArray>>&& callback,
    nx::utils::AsyncHandlerExecutor executor,
    const nx::network::http::HttpHeaders& customHeaders);

Handle ServerConnection::getPluginInformation(
    const nx::Uuid& serverId,
    GetCallback callback,
    nx::utils::AsyncHandlerExecutor executor)
{
    return executeGet("/api/pluginInfo", {}, std::move(callback), executor, serverId);
}

Handle ServerConnection::testEmailSettings(
    const nx::vms::api::EmailSettings& settings,
    Callback<ResultWithData<QnTestEmailSettingsReply>>&& callback,
    nx::utils::AsyncHandlerExecutor executor,
    std::optional<nx::Uuid> proxyToServer)
{
    return executePost(
        "/api/testEmailSettings",
        QJson::serialized(settings),
        std::move(callback),
        executor,
        proxyToServer);
}

Handle ServerConnection::testEmailSettings(
    Callback<ResultWithData<QnTestEmailSettingsReply>>&& callback,
    nx::utils::AsyncHandlerExecutor executor,
    std::optional<nx::Uuid> proxyToServer)
{
    return executePost(
        "/api/testEmailSettings",
        /*messageBody*/ QByteArray(),
        std::move(callback),
        executor,
        proxyToServer);
}

Handle ServerConnection::getStorageStatus(
    const nx::Uuid& serverId,
    const QString& path,
    Callback<ResultWithData<StorageStatusReply>>&& callback,
    nx::utils::AsyncHandlerExecutor executor)
{
    nx::network::rest::Params params;
    params.insert("path", path);
    return executeGet("/api/storageStatus", params, std::move(callback), executor, serverId);
}

Handle ServerConnection::checkStoragePath(
    const QString& path,
    Callback<ErrorOrData<nx::vms::api::StorageSpaceDataWithDbInfoV3>>&& callback,
    nx::utils::AsyncHandlerExecutor executor)
{
    nx::network::rest::Params params;
    // Method prepareUrl calls params.toUrlQuery() which uses QUrlQuery::setQueryItems()
    // which has requirement: `The keys and values are expected to be in percent-encoded form.`
    // So it is required to encode path manually because it can be a url with many
    // symbols like '@', ':', '%' and others or even be a path with utf-8 symbols.
    params.insert("path", nx::reflect::urlencoded::serialize(path));
    return executeGet(
        "/rest/v4/servers/this/storages/*/check",
        params,
        std::move(callback),
        executor);
}

Handle ServerConnection::setStorageEncryptionPassword(
    const QString& password,
    bool makeCurrent,
    const QByteArray& salt,
    PostCallback&& callback,
    nx::utils::AsyncHandlerExecutor executor)
{
    nx::vms::api::StorageEncryptionData data;
    data.password = password;
    data.makeCurrent = makeCurrent;
    data.salt = salt;

    return executePost(
        "/rest/v1/system/storageEncryption",
        QJson::serialized(data),
        std::move(callback),
        executor);
}

Handle ServerConnection::getSystemIdFromServer(
    const nx::Uuid& serverId,
    Callback<QString>&& callback,
    nx::utils::AsyncHandlerExecutor executor)
{
    auto internalCallback =
        [callback=std::move(callback)](
            bool success, Handle requestId, QByteArray result,
            const nx::network::http::HttpHeaders& /*headers*/)
        {
            callback(success, requestId, QString::fromUtf8(result));
        };

    auto request = prepareRequest(
        nx::network::http::Method::get,
        prepareUrl("/api/getSystemId", /*params*/ {}));

    proxyRequestUsingServer(request, serverId);

    return d->executeRequest(
        /*helper*/ nullptr,
        request,
        std::make_unique<RawResultContext>(),
        RawResultContext::wrapCallback(std::move(internalCallback)),
        executor);
}

Handle ServerConnection::doCameraDiagnosticsStep(
    const nx::Uuid& serverId,
    const nx::Uuid& cameraId,
    CameraDiagnostics::Step::Value previousStep,
    Callback<ResultWithData<QnCameraDiagnosticsReply>>&& callback,
    nx::utils::AsyncHandlerExecutor executor)
{
    nx::network::rest::Params params;
    params.insert("cameraId", cameraId);
    params.insert("type", CameraDiagnostics::Step::toString(previousStep));

    return executeGet("/api/doCameraDiagnosticsStep", params, std::move(callback), executor, serverId);
}

Handle ServerConnection::ldapAuthenticateAsync(
    const nx::vms::api::Credentials& credentials,
    bool localOnly,
    LdapAuthenticateCallback&& callback,
    nx::utils::AsyncHandlerExecutor executor)
{
    nx::network::rest::Params params;
    if (localOnly)
        params.insert("_local", true);
    auto request = prepareRequest(
        nx::network::http::Method::post,
        prepareUrl("/rest/v3/ldap/authenticate", params),
        nx::network::http::header::ContentType::kJson.toString(),
        nx::reflect::json::serialize(credentials));

    return d->executeRequest(
        /*helper*/ nullptr,
        request,
        std::make_unique<RawResultContext>(),
        RawResultContext::wrapCallback([callback = std::move(callback)](
            bool success,
            Handle requestId,
            QByteArray body,
            const nx::network::http::HttpHeaders& httpHeaders)
        {
            using AuthResult = nx::network::rest::AuthResult;
            AuthResult authResult = AuthResult::Auth_LDAPConnectError;
            const auto authResultString = nx::network::http::getHeaderValue(
                httpHeaders, Qn::AUTH_RESULT_HEADER_NAME);
            if (!authResultString.empty())
                nx::reflect::fromString<AuthResult>(authResultString, &authResult);
            if (!success)
            {
                nx::network::rest::Result result;
                QJson::deserialize(body, &result);
                callback(requestId, std::unexpected(std::move(result)), authResult);
                return;
            }

            nx::vms::api::UserModelV3 user;
            QJson::deserialize(body, &user);
            callback(requestId, std::move(user), authResult);
        }),
        executor);
}

Handle ServerConnection::testLdapSettingsAsync(
    const nx::vms::api::LdapSettings& settings,
    Callback<ErrorOrData<std::vector<QString>>>&& callback,
    nx::utils::AsyncHandlerExecutor executor)
{
    return executePost(
        "/rest/v3/ldap/test",
        nx::reflect::json::serialize(settings),
        std::move(callback),
        executor);
}

Handle ServerConnection::setLdapSettingsAsync(
    const nx::vms::api::LdapSettings& settings,
    nx::vms::common::SessionTokenHelperPtr tokenHelper,
    Callback<ErrorOrData<nx::vms::api::LdapSettings>>&& callback,
    nx::utils::AsyncHandlerExecutor executor)
{
    auto request = prepareRequest(
        nx::network::http::Method::put,
        prepareUrl(
            "/rest/v3/ldap/settings",
            /*params*/ {}),
        Qn::serializationFormatToHttpContentType(Qn::SerializationFormat::json),
        nx::reflect::json::serialize(settings));

    using Context = ResultContext<ErrorOrData<nx::vms::api::LdapSettings>>;

    return d->executeRequest(
        tokenHelper,
        request,
        std::make_unique<Context>(),
        Context::wrapCallback(std::move(callback)),
        executor);
}

Handle ServerConnection::modifyLdapSettingsAsync(
    const nx::vms::api::LdapSettings& settings,
    nx::vms::common::SessionTokenHelperPtr tokenHelper,
    Callback<ErrorOrData<nx::vms::api::LdapSettings>>&& callback,
    nx::utils::AsyncHandlerExecutor executor)
{
    auto request = prepareRequest(
        nx::network::http::Method::patch,
        prepareUrl(
            "/rest/v3/ldap/settings",
            /*params*/ {}),
        Qn::serializationFormatToHttpContentType(Qn::SerializationFormat::json),
        nx::reflect::json::serialize(settings));

    using Context = ResultContext<ErrorOrData<nx::vms::api::LdapSettings>>;

    return d->executeRequest(
        tokenHelper,
        request,
        std::make_unique<Context>(),
        Context::wrapCallback(std::move(callback)),
        executor);
}

Handle ServerConnection::loginInfoAsync(
    const QString& login,
    bool localOnly,
    Callback<ErrorOrData<nx::vms::api::LoginUser>>&& callback,
    nx::utils::AsyncHandlerExecutor executor)
{
    nx::network::rest::Params params;
    if (localOnly)
        params.insert("_local", true);
    return executeGet("/rest/v3/login/users/" + login, params, std::move(callback), executor);
}

Handle ServerConnection::getLdapSettingsAsync(
    Callback<ErrorOrData<nx::vms::api::LdapSettings>>&& callback,
    nx::utils::AsyncHandlerExecutor executor)
{
    return executeGet("/rest/v3/ldap/settings", {}, std::move(callback), executor);
}

Handle ServerConnection::getLdapStatusAsync(
    Callback<ErrorOrData<nx::vms::api::LdapStatus>>&& callback,
    nx::utils::AsyncHandlerExecutor executor)
{
    return executeGet("/rest/v3/ldap/sync", {}, std::move(callback), executor);
}

Handle ServerConnection::syncLdapAsync(
    nx::vms::common::SessionTokenHelperPtr tokenHelper,
    Callback<ErrorOrEmpty>&& callback,
    nx::utils::AsyncHandlerExecutor executor)
{
    auto request = prepareRequest(
        nx::network::http::Method::post,
        prepareUrl(
            "/rest/v3/ldap/sync",
            /*params*/ {}),
        Qn::serializationFormatToHttpContentType(Qn::SerializationFormat::json));

    using Context = ResultContext<ErrorOrEmpty>;

    return d->executeRequest(
        tokenHelper,
        request,
        std::make_unique<Context>(),
        Context::wrapCallback(std::move(callback)),
        executor);
}

Handle ServerConnection::resetLdapAsync(
    nx::vms::common::SessionTokenHelperPtr tokenHelper,
    Callback<ErrorOrEmpty>&& callback,
    nx::utils::AsyncHandlerExecutor executor)
{
    auto request = prepareRequest(
        nx::network::http::Method::delete_,
        prepareUrl(
            "/rest/v3/ldap/settings",
            /*params*/ {}),
        Qn::serializationFormatToHttpContentType(Qn::SerializationFormat::json));

    using Context = ResultContext<ErrorOrEmpty>;

    return d->executeRequest(
        tokenHelper,
        request,
        std::make_unique<Context>(),
        Context::wrapCallback(std::move(callback)),
        executor);
}

Handle ServerConnection::saveUserAsync(
    bool newUser,
    const nx::vms::api::UserModelV3& userData,
    nx::vms::common::SessionTokenHelperPtr tokenHelper,
    Callback<ErrorOrData<nx::vms::api::UserModelV3>>&& callback,
    nx::utils::AsyncHandlerExecutor executor)
{
    auto request = prepareRequest(
        newUser ? nx::network::http::Method::put : nx::network::http::Method::patch,
        prepareUrl(
            QString("/rest/v4/users/%1").arg(userData.id.toSimpleString()),
            /*params*/ {}),
        Qn::serializationFormatToHttpContentType(Qn::SerializationFormat::json),
        nx::reflect::json::serialize(userData));

    using Context = ResultContext<ErrorOrData<nx::vms::api::UserModelV3>>;

    return d->executeRequest(
        tokenHelper,
        request,
        std::make_unique<Context>(),
        Context::wrapCallback(std::move(callback)),
        executor);
}

 Handle ServerConnection::patchUserSettings(
    nx::Uuid id,
    const nx::vms::api::UserSettings& settings,
    Callback<ErrorOrData<nx::vms::api::UserModelV3>>&& callback,
    nx::utils::AsyncHandlerExecutor executor)
{
    QJsonValue serializedSettings;
    QJson::serialize(settings, &serializedSettings);

    auto request = prepareRequest(
        nx::network::http::Method::patch,
        prepareUrl(
            QString("/rest/v4/users/%1").arg(id.toSimpleString()),
            /*params*/ {}),
        Qn::serializationFormatToHttpContentType(Qn::SerializationFormat::json),
        nx::reflect::json::serialize(QJsonObject{{"settings", serializedSettings}}));

    using Context = ResultContext<ErrorOrData<nx::vms::api::UserModelV3>>;

    return d->executeRequest(
        /*tokenHelper*/ nullptr,
        request,
        std::make_unique<Context>(),
        Context::wrapCallback(std::move(callback)),
        executor);
}

Handle ServerConnection::removeUserAsync(
    const nx::Uuid& userId,
    nx::vms::common::SessionTokenHelperPtr tokenHelper,
    Callback<ErrorOrEmpty>&& callback,
    nx::utils::AsyncHandlerExecutor executor)
{
    auto request = prepareRequest(
        nx::network::http::Method::delete_,
        prepareUrl(
            QString("/rest/v4/users/%1").arg(userId.toSimpleString()),
            /*params*/ {}));

    using Context = ResultContext<ErrorOrEmpty>;

    return d->executeRequest(
        tokenHelper,
        request,
        std::make_unique<Context>(),
        Context::wrapCallback(std::move(callback)),
        executor);
}

Handle ServerConnection::saveGroupAsync(
    bool newGroup,
    const nx::vms::api::UserGroupModel& groupData,
    nx::vms::common::SessionTokenHelperPtr tokenHelper,
    Callback<ErrorOrData<nx::vms::api::UserGroupModel>>&& callback,
    nx::utils::AsyncHandlerExecutor executor)
{
    auto request = prepareRequest(
        newGroup ? nx::network::http::Method::put : nx::network::http::Method::patch,
        prepareUrl(
            QString("/rest/v4/userGroups/%1").arg(groupData.id.toSimpleString()),
            /*params*/ {}),
        Qn::serializationFormatToHttpContentType(Qn::SerializationFormat::json),
        nx::reflect::json::serialize(groupData));

    using Context = ResultContext<ErrorOrData<nx::vms::api::UserGroupModel>>;

    return d->executeRequest(
        tokenHelper,
        request,
        std::make_unique<Context>(),
        Context::wrapCallback(std::move(callback)),
        executor);
}

Handle ServerConnection::removeGroupAsync(
    const nx::Uuid& groupId,
    nx::vms::common::SessionTokenHelperPtr tokenHelper,
    Callback<ErrorOrEmpty>&& callback,
    nx::utils::AsyncHandlerExecutor executor)
{
    auto request = prepareRequest(
        nx::network::http::Method::delete_,
        prepareUrl(
            QString("/rest/v4/userGroups/%1").arg(groupId.toSimpleString()),
            /*params*/ {}));

    using Context = ResultContext<ErrorOrEmpty>;

    return d->executeRequest(
        tokenHelper,
        request,
        std::make_unique<Context>(),
        Context::wrapCallback(std::move(callback)),
        executor);
}

Handle ServerConnection::getStoredFiles(
    Callback<ErrorOrData<nx::vms::api::StoredFileDataList>>&& callback,
    nx::utils::AsyncHandlerExecutor executor)
{
    return executeGet("/rest/v4/storedFiles", {}, std::move(callback), executor);
}

Handle ServerConnection::getStoredFile(const QString& filePath,
    Callback<ErrorOrData<nx::vms::api::StoredFileData>>&& callback,
    nx::utils::AsyncHandlerExecutor executor)
{
    return executeGet(
        nx::format("/rest/v4/storedFiles/%1", filePath), {}, std::move(callback), executor);
}

Handle ServerConnection::putStoredFile(const nx::vms::api::StoredFileData& storedFileData,
    Callback<ErrorOrData<nx::vms::api::StoredFileData>>&& callback,
    nx::utils::AsyncHandlerExecutor executor)
{
    return executePut(nx::format("/rest/v4/storedFiles/%1", storedFileData.path),
        {},
        Qn::serializationFormatToHttpContentType(Qn::SerializationFormat::json),
        nx::reflect::json::serialize(storedFileData),
        std::move(callback),
        executor);
}

Handle ServerConnection::deleteStoredFile(const QString& filePath,
    Callback<ErrorOrEmpty>&& callback,
    nx::utils::AsyncHandlerExecutor executor)
{
    return executeDelete(
        nx::format("/rest/v4/storedFiles/%1", filePath), {}, std::move(callback), executor);
}

Handle ServerConnection::createTicket(
    const nx::Uuid& targetServerId,
    Callback<ErrorOrData<nx::vms::api::LoginSession>> callback,
    nx::utils::AsyncHandlerExecutor executor)
{
    auto request = prepareRequest(
        nx::network::http::Method::post,
        prepareUrl("/rest/v3/login/tickets", {}),
        Qn::serializationFormatToHttpContentType(Qn::SerializationFormat::json),
        {});

    using Context = ResultContext<ErrorOrData<nx::vms::api::LoginSession>>;

    proxyRequestUsingServer(request, targetServerId);

    return d->executeRequest(
        /*tokenHelper*/ nullptr,
        request,
        std::make_unique<Context>(),
        Context::wrapCallback(std::move(callback)),
        executor);
}

Handle ServerConnection::getCurrentSession(
    Callback<ErrorOrData<nx::vms::api::LoginSession>> callback,
    nx::utils::AsyncHandlerExecutor executor)
{
    return executeGet(
        "/rest/v1/login/sessions/current",
        {},
        std::move(callback),
        executor);
}

Handle ServerConnection::loginAsync(
    const nx::vms::api::LoginSessionRequest& data,
    Callback<ErrorOrData<nx::vms::api::LoginSession>> callback,
    nx::utils::AsyncHandlerExecutor executor)
{
    return executePost(
        "/rest/v1/login/sessions",
        nx::reflect::json::serialize(data),
        std::move(callback),
        executor);
}

Handle ServerConnection::loginAsync(
    const nx::vms::api::TemporaryLoginSessionRequest& data,
    Callback<ErrorOrData<nx::vms::api::LoginSession>> callback,
    nx::utils::AsyncHandlerExecutor executor)
{
    return executePost(
        "/rest/v3/login/temporaryToken",
        nx::reflect::json::serialize(data),
        std::move(callback),
        executor);
}

Handle ServerConnection::replaceDevice(
    const nx::Uuid& deviceToBeReplacedId,
    const QString& replacementDevicePhysicalId,
    bool returnReportOnly,
    Callback<nx::vms::api::DeviceReplacementResponse>&& callback,
    nx::utils::AsyncHandlerExecutor executor)
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
            const nx::network::http::HttpHeaders& /*headers*/)
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

    return d->executeRequest(
        /*helper*/ nullptr,
        request,
        std::make_unique<RawResultContext>(),
        RawResultContext::wrapCallback(std::move(internal_callback)),
        executor);
}

Handle ServerConnection::undoReplaceDevice(
    const nx::Uuid& deviceId,
    PostCallback&& callback,
    nx::utils::AsyncHandlerExecutor executor)
{
    return executeDelete(
        nx::format("/rest/v2/devices/%1/replace", deviceId),
        nx::network::rest::Params(),
        std::move(callback),
        executor,
        {});
}

Handle ServerConnection::recordedTimePeriods(
    const QnChunksRequestData& requestData,
    Callback<MultiServerPeriodDataList>&& callback,
    nx::utils::AsyncHandlerExecutor executor)
{
    QnChunksRequestData fixedFormatRequest(requestData);
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

    auto request = prepareRequest(
        nx::network::http::Method::get,
        prepareUrl("/ec2/recordedTimePeriods", fixedFormatRequest.toParams()));
    request.priority = nx::network::http::ClientPool::Request::Priority::high;

    return d->executeRequest(
        /*helper*/ nullptr,
        request,
        std::make_unique<RawResultContext>(),
        RawResultContext::wrapCallback(std::move(internalCallback)),
        executor);
}

Handle ServerConnection::getExtendedPluginInformation(
    Callback<nx::vms::api::ExtendedPluginInfoByServer>&& callback,
    nx::utils::AsyncHandlerExecutor executor)
{
    return executeGet(
        "/ec2/pluginInfo",
        {},
        Callback<nx::network::rest::JsonResult>(
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
        executor);
}

Handle ServerConnection::debug(
    const QString& action, const QString& value, PostCallback callback, nx::utils::AsyncHandlerExecutor executor)
{
    return executeGet("/api/debug", {{action, value}}, std::move(callback), executor);
}

Handle ServerConnection::getLookupLists(
    Callback<ErrorOrData<nx::vms::api::LookupListDataList>>&& callback,
    nx::utils::AsyncHandlerExecutor executor)
{
    return executeGet("/rest/v3/lookupLists", {}, std::move(callback), executor);
}

Handle ServerConnection::saveLookupList(
    const nx::vms::api::LookupListData& lookupList,
    Callback<ErrorOrEmpty> callback,
    nx::utils::AsyncHandlerExecutor executor)
{
    return executePut(
        nx::format("/rest/v4/lookupLists/%1").arg(lookupList.id),
        {},
        Qn::serializationFormatToHttpContentType(Qn::SerializationFormat::json),
        QByteArray::fromStdString(nx::reflect::json::serialize(lookupList)),
        std::move(callback),
        executor);
}

// --------------------------- private implementation -------------------------------------

QUrl ServerConnection::prepareUrl(const QString& path, const nx::network::rest::Params& params) const
{
    QUrl result;
    result.setPath(path);
    result.setQuery(params.toUrlQuery());
    return result;
}

template<typename ResultType>
Handle ServerConnection::executeGet(
    const QString& path,
    const nx::network::rest::Params& params,
    Callback<ResultType> callback,
    nx::utils::AsyncHandlerExecutor executor,
    std::optional<nx::Uuid> proxyToServer,
    std::optional<Timeouts> timeouts)
{
    auto request = this->prepareRequest(nx::network::http::Method::get, prepareUrl(path, params));
    if (proxyToServer)
        proxyRequestUsingServer(request, *proxyToServer);

    return d->executeRequest(
        /*helper*/ nullptr,
        request,
        std::make_unique<ResultContext<ResultType>>(),
        ResultContext<ResultType>::wrapCallback(std::move(callback)),
        executor,
        timeouts);
}

template <typename ResultType>
Handle ServerConnection::executePost(
    const QString& path,
    const nx::network::rest::Params& params,
    Callback<ResultType> callback,
    nx::utils::AsyncHandlerExecutor executor,
    std::optional<nx::Uuid> proxyToServer)
{
    return executePost(
        path,
        QJson::serialized(params.toJson()),
        std::move(callback),
        executor,
        proxyToServer);
}

template <typename ResultType>
Handle ServerConnection::executePost(
    const QString& path,
    const nx::String& messageBody,
    Callback<ResultType>&& callback,
    nx::utils::AsyncHandlerExecutor executor,
    std::optional<nx::Uuid> proxyToServer)
{
    return executePost(
        path,
        /*params*/ {},
        nx::network::http::header::ContentType::kJson.toString(),
        messageBody,
        std::move(callback),
        executor,
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
    nx::utils::AsyncHandlerExecutor executor,
    std::optional<Timeouts> timeouts,
    std::optional<nx::Uuid> proxyToServer)
{
    auto request = this->prepareRequest(
        nx::network::http::Method::post, prepareUrl(path, params), contentType, messageBody);

    if (proxyToServer)
        proxyRequestUsingServer(request, *proxyToServer);

    return d->executeRequest(
        /*helper*/ nullptr,
        request,
        std::make_unique<ResultContext<ResultType>>(),
        ResultContext<ResultType>::wrapCallback(std::move(callback)),
        executor,
        timeouts);
}

template <typename ResultType>
Handle ServerConnection::executePut(
    const QString& path,
    const nx::network::rest::Params& params,
    const nx::String& contentType,
    const nx::String& messageBody,
    Callback<ResultType> callback,
    nx::utils::AsyncHandlerExecutor executor,
    std::optional<nx::Uuid> proxyToServer)
{
    auto request = prepareRequest(
        nx::network::http::Method::put, prepareUrl(path, params), contentType, messageBody);
    if (proxyToServer)
        proxyRequestUsingServer(request, *proxyToServer);

    return d->executeRequest(
        /*helper*/ nullptr,
        request,
        std::make_unique<ResultContext<ResultType>>(),
        ResultContext<ResultType>::wrapCallback(std::move(callback)),
        executor);
}

template <typename ResultType>
Handle ServerConnection::executePatch(
    const QString& path,
    const nx::network::rest::Params& params,
    const nx::String& contentType,
    const nx::String& messageBody,
    Callback<ResultType> callback,
    nx::utils::AsyncHandlerExecutor executor)
{
    auto request = prepareRequest(
        nx::network::http::Method::patch, prepareUrl(path, params), contentType, messageBody);

    return d->executeRequest(request, std::move(callback), std::move(executor));
}

template <typename ResultType>
Handle ServerConnection::executeDelete(
    const QString& path,
    const nx::network::rest::Params& params,
    Callback<ResultType> callback,
    nx::utils::AsyncHandlerExecutor executor,
    std::optional<nx::Uuid> proxyToServer)
{
    auto request = prepareRequest(nx::network::http::Method::delete_, prepareUrl(path, params));
    if (proxyToServer)
        proxyRequestUsingServer(request, *proxyToServer);

    return d->executeRequest(
        /*helper*/ nullptr,
        request,
        std::make_unique<ResultContext<ResultType>>(),
        ResultContext<ResultType>::wrapCallback(std::move(callback)),
        executor);
}

// Coroutine style request.
nx::coro::Task<
    std::tuple<
        std::unique_ptr<BaseResultContext>,
        rest::Handle>>
ServerConnection::Private::executeRequestAsync(
    nx::network::http::ClientPool::Request request,
    std::unique_ptr<BaseResultContext> requestContext,
    std::optional<Timeouts> timeouts,
    nx::vms::common::SessionTokenHelperPtr helper,
    HandleCallback onSuspend)
{
    co_await guard(q);

    auto context = co_await executeRequestAwaitable(
        request,
        timeouts,
        std::move(onSuspend),
        logTag);

    rest::Handle originalHandle = context->handle;

    // Asio thread.

    requestContext->parse(
        Qn::serializationFormatFromHttpContentType(context->response.contentType),
        context->response.messageBody,
        context->systemError,
        context->getStatusLine(),
        context->response.headers);

    // RawResultContext is not intended to be used with requests, which potentially could require
    // a fresh session. We could add an assert here, validating the context type if a helper is
    // provided.
    if (requestContext->isSessionExpired() && helper)
    {
        co_await nx::utils::AsyncHandlerExecutor(qApp);

        std::optional<nx::network::http::AuthToken> updatedToken;
        {
            NX_MUTEX_LOCKER lock(&mutex);
            // Check if the token was updated while the request was in flight.
            if (request.credentials->authToken != directConnect->credentials.authToken)
                updatedToken = directConnect->credentials.authToken;
        }

        if (!updatedToken)
        {
            updatedToken = co_await rerunner.getNewToken(helper);
            if (updatedToken)
            {
                NX_MUTEX_LOCKER lock(&mutex);
                directConnect->credentials.authToken = *updatedToken;
            }
        }
        if (updatedToken)
        {
            // GUI or ASIO thread.
            auto fixedRequest = requestContext->fixup(request);
            fixedRequest.credentials->authToken = *updatedToken;

            // Remove substitution even if the task is cancelled.
            auto removeSubstitution = nx::utils::makeScopeGuard(
                nx::utils::AsyncHandlerExecutor(qApp).bind(
                    [this, originalHandle]
                    {
                        NX_MUTEX_LOCKER lock(&mutex);
                        substitutions.erase(originalHandle);
                    }));

            context = co_await executeRequestAwaitable(
                fixedRequest, timeouts,
                [originalHandle, this](rest::Handle handle)
                {
                    NX_MUTEX_LOCKER lock(&mutex);
                    substitutions[originalHandle] = handle;
                },
                logTag);

            removeSubstitution.fire(); //< Remove substitution early.

            // Asio thread.
            requestContext->parse(
                Qn::serializationFormatFromHttpContentType(context->response.contentType),
                context->response.messageBody,
                context->systemError,
                context->getStatusLine(),
                context->response.headers);
        }
    }

    co_return {std::move(requestContext), originalHandle};
}

// Callback style request.
Handle ServerConnection::Private::executeRequest(
    nx::vms::common::SessionTokenHelperPtr helper,
    const nx::network::http::ClientPool::Request& request,
    std::unique_ptr<BaseResultContext> requestContext,
    RequestCallback callback,
    nx::utils::AsyncHandlerExecutor executor,
    std::optional<Timeouts> timeouts)
{
    rest::Handle handle = {};

    if (!request.isValid())
    {
        NX_VERBOSE(logTag, "<%1> %2 %3", handle, request.method, request.url);
        return {};
    }

    [this](
        const nx::network::http::ClientPool::Request& request,
        std::unique_ptr<BaseResultContext> requestContext,
        RequestCallback callback,
        std::optional<Timeouts> timeouts,
        nx::vms::common::SessionTokenHelperPtr helper,
        HandleCallback onHandle) -> FireAndForget
    {
        co_await guard(q);

        const auto tag = this->logTag; //< Store this data on coroutine frame.

        QElapsedTimer timer;
        timer.start();

        auto [context, handle] = co_await executeRequestAsync(
            request,
            std::move(requestContext),
            timeouts,
            helper,
            std::move(onHandle));

        const auto elapsedMs = timer.elapsed();
        if (context->isSuccess())
            NX_VERBOSE(tag, "<%1>: Reply success for %2ms", handle, elapsedMs);
        else
            NX_VERBOSE(tag, "<%1>: Reply failed for %2ms", handle, elapsedMs);

        if (callback)
            callback(std::move(context), handle);

    }(
        request,
        std::move(requestContext),
        executor.bind(std::move(callback)),
        timeouts,
        helper,
        [&handle](rest::Handle h) { handle = h; });

    NX_VERBOSE(logTag, "<%1> %2 %3", handle, request.method, request.url);
    return handle;
}

void ServerConnection::cancelRequest(Handle requestId)
{
    std::optional<Handle> actualId;
    {
        // Check if we had re-send this request with updated credentials.
        NX_MUTEX_LOCKER lock(&d->mutex);

        if (auto it = d->substitutions.find(requestId); it != d->substitutions.end())
        {
            actualId = it->second;
            d->pendingRequests.erase(*actualId);
        }

        d->pendingRequests.erase(requestId);
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
    const auto localPeerType = nx::vms::common::appContext()->localPeerType();
    if (PeerData::isClient(localPeerType))
        return connection->credentials();

    NX_ASSERT(PeerData::isServer(localPeerType), "Unexpected peer type");
    return targetServer->credentials();
}

bool setupAuth(
    const nx::vms::common::SystemContext* systemContext,
    const nx::Uuid& auditId,
    const nx::Uuid& serverId,
    nx::network::http::ClientPool::Request& request,
    const QUrl& url,
    const nx::Uuid& userId)
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
        Qn::EC2_RUNTIME_GUID_HEADER_NAME, auditId.toSimpleStdString());

    const QnRoute route = QnRouter::routeTo(server);

    if (route.reverseConnect)
    {
        if (nx::vms::api::PeerData::isClient(nx::vms::common::appContext()->localPeerType()))
        {
            const auto connection = systemContext->messageBusConnection();
            if (!NX_ASSERT(connection))
                return false;

            const auto address = connection->address();
            request.url.setHost(address.address);
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
                const auto url = nx::Url(currentServer->getUrl());
                if (url.port() > 0)
                    request.url.setPort(url.port());
            }
        }
    }
    else if (!route.addr.isNull())
    {
        request.url.setHost(route.addr.address);
        request.url.setPort(route.addr.port);
    }

    // TODO: #sivanov Only client-side connection is actually used.
    const auto connection = systemContext->messageBusConnection();
    if (!connection)
        return false;

    request.headers.emplace(Qn::SERVER_GUID_HEADER_NAME, server->getId().toSimpleStdString());
    request.credentials = getRequestCredentials(connection, server);

    QString userName;
    if (!userId.isNull())
    {
        if (auto user = systemContext->resourcePool()->getResourceById<QnUserResource>(userId))
            userName = user->getName();
    }
    else
    {
        userName = QString::fromStdString(request.credentials->username);
    }

    if (!userName.isEmpty())
        request.headers.emplace(Qn::CUSTOM_USERNAME_HEADER_NAME, userName.toLower().toUtf8());
    if (!route.gatewayId.isNull())
        request.gatewayId = route.gatewayId;

    return true;
}

void setupAuthDirect(
    nx::network::http::ClientPool::Request& request,
    const nx::Uuid& auditId,
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
    request.headers.emplace(Qn::EC2_RUNTIME_GUID_HEADER_NAME, auditId.toSimpleStdString());

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
                d->auditId,
                d->directConnect->address,
                d->directConnect->credentials,
                url.path(),
                url.query());
            isDirect = authIsSet = true;
        }
    }

    if (!isDirect)
        authIsSet = setupAuth(d->systemContext, d->auditId, d->serverId, request, url, d->userId);

    if (!authIsSet)
        return nx::network::http::ClientPool::Request();

    request.method = method;
    request.contentType = contentType;
    request.messageBody = messageBody;
    QString locale = nx::i18n::TranslationManager::getCurrentThreadLocale();
    if (locale.isEmpty())
        locale = nx::vms::common::appContext()->locale();
    request.headers.emplace(nx::network::http::header::kAcceptLanguage, locale.toStdString());
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

nx::network::http::ClientPool::ContextPtr ServerConnection::prepareContext(
    const nx::network::http::ClientPool::Request& request,
    nx::MoveOnlyFunc<void (ContextPtr)> callback,
    std::optional<Timeouts> timeouts)
{
    auto certificateVerifier = d->directConnect
        ? d->directConnect->certificateVerifier.data()
        : d->systemContext->certificateVerifier();
    if (!NX_ASSERT(certificateVerifier))
        return {};

    auto context = d->httpClientPool->createContext(
        d->certificateFuncId,
        certificateVerifier->makeAdapterFunc(
            request.gatewayId.value_or(d->serverId), request.url));
    context->request = request;
    context->completionFunc = std::move(callback);
    if (timeouts)
        context->timeouts = *timeouts;
    context->setTargetThread(nullptr); //< Callback runs in target thread via AsyncHandlerExecutor.

    return context;
}

Handle ServerConnection::sendRequest(const ContextPtr& context)
{
    if (!context)
        return 0;

    auto metrics = nx::vms::common::appContext()->metrics();
    const auto total = ++metrics->totalServerRequests();

    Handle requestId = d->httpClientPool->sendRequest(context);
    NX_VERBOSE(d->logTag, "<%1> %2: %3", requestId, metrics->totalServerRequests.name(), total);

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
