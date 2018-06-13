#include "remote_connection_factory.h"

#include <functional>

#include <api/global_settings.h>

#include <common/static_common_module.h>

#include <nx/utils/thread/mutex.h>
#include <nx/network/http/auth_tools.h>
#include <nx/network/deprecated/simple_http_client.h>
#include <nx/network/address_resolver.h>

#include <utils/common/app_info.h>
#include <nx/utils/concurrent.h>

#include "compatibility/old_ec_connection.h"
#include "ec2_thread_pool.h"
#include "remote_ec_connection.h"
#include <transaction/message_bus_adapter.h>
#include <nx/time_sync/client_time_sync_manager.h>

namespace ec2 {

RemoteConnectionFactory::RemoteConnectionFactory(
    QnCommonModule* commonModule,
    Qn::PeerType peerType,
    bool isP2pMode)
:
    AbstractECConnectionFactory(commonModule),
    m_jsonTranSerializer(new QnJsonTransactionSerializer()),
    m_ubjsonTranSerializer(new QnUbjsonTransactionSerializer()),
    m_timeSynchronizationManager(new nx::time_sync::ClientTimeSyncManager(
        commonModule)),
    m_terminated(false),
    m_runningRequests(0),
    m_sslEnabled(false),
    m_remoteQueryProcessor(new ClientQueryProcessor(commonModule)),
    m_peerType(peerType)
{
    m_bus.reset(new ThreadsafeMessageBusAdapter(
        Qn::PeerType(),
        commonModule,
        m_jsonTranSerializer.get(),
        m_ubjsonTranSerializer.get()));
}

void RemoteConnectionFactory::shutdown()
{
    // Have to do it before m_transactionMessageBus destruction since TimeSynchronizationManager
    // uses QnTransactionMessageBus.
	// todo: introduce server and client TimeSynchronizationManager
    pleaseStop();
    join();
}

RemoteConnectionFactory::~RemoteConnectionFactory()
{
}

void RemoteConnectionFactory::pleaseStop()
{
    QnMutexLocker lk(&m_mutex);
    m_terminated = true;
}

void RemoteConnectionFactory::join()
{
    // Cancelling all ongoing requests.
    m_remoteQueryProcessor->pleaseStopSync();
}

// Implementation of AbstractECConnectionFactory::testConnectionAsync
int RemoteConnectionFactory::testConnectionAsync(
    const nx::utils::Url &addr,
    impl::TestConnectionHandlerPtr handler)
{
    nx::utils::Url url = addr;
    url.setUserName(url.userName().toLower());

    QUrlQuery query(url.toQUrl());
    query.removeQueryItem(lit("format"));
    query.addQueryItem(lit("format"), QnLexical::serialized(Qn::JsonFormat));
    url.setQuery(query);

    if (url.isEmpty())
        return testDirectConnection(url, handler);
    else
        return testRemoteConnection(url, handler);
}

// Implementation of AbstractECConnectionFactory::connectAsync
int RemoteConnectionFactory::connectAsync(
    const nx::utils::Url& addr,
    const nx::vms::api::ClientInfoData& clientInfo,
    impl::ConnectHandlerPtr handler)
{
    nx::utils::Url url = addr;
    url.setUserName(url.userName().toLower());

    if (ApiPeerData::isMobileClient(qnStaticCommon->localPeerType()))
    {
        QUrlQuery query(url.toQUrl());
        query.removeQueryItem(lit("format"));
        query.addQueryItem(lit("format"), QnLexical::serialized(Qn::JsonFormat));
        url.setQuery(query);
    }

	return establishConnectionToRemoteServer(url, handler, clientInfo);
}

void RemoteConnectionFactory::setConfParams(std::map<QString, QVariant> confParams)
{
    m_settingsInstance.loadParams(std::move(confParams));
}

int RemoteConnectionFactory::establishConnectionToRemoteServer(
    const nx::utils::Url& addr,
    impl::ConnectHandlerPtr handler,
    const nx::vms::api::ClientInfoData& clientInfo)
{
    const int reqId = generateRequestID();

#if 0 // TODO: #ak Return existing connection, if any.
    {
        QnMutexLocker lk(&m_mutex);
        auto it = m_urlToConnection.find(addr);
        if (it != m_urlToConnection.end())
            AbstractECConnectionPtr connection = it->second.second;
    }
#endif // 0

    nx::vms::api::ConnectionData loginInfo;
    loginInfo.login = addr.userName();
    loginInfo.passwordHash = nx::network::http::calcHa1(
        loginInfo.login.toLower(), nx::network::AppInfo::realm(), addr.password());
    loginInfo.clientInfo = clientInfo;

    {
        QnMutexLocker lk(&m_mutex);
        if (m_terminated)
            return INVALID_REQ_ID;
        ++m_runningRequests;
    }

    const auto info = QString::fromUtf8(QJson::serialized(clientInfo) );
    NX_LOG(lit("%1 to %2 with %3").arg(Q_FUNC_INFO).arg(addr.toString(QUrl::RemovePassword)).arg(info),
            cl_logDEBUG1);

    auto func =
        [this, reqId, addr, handler](
            ErrorCode errorCode, const QnConnectionInfo& connectionInfo)
        {
            remoteConnectionFinished(reqId, errorCode, connectionInfo, addr, handler);
        };
    m_remoteQueryProcessor->processQueryAsync<nx::vms::api::ConnectionData, QnConnectionInfo>(
        addr, ApiCommand::connect, loginInfo, func);
    return reqId;
}

void RemoteConnectionFactory::tryConnectToOldEC(const nx::utils::Url& ecUrl,
    impl::ConnectHandlerPtr handler, int reqId)
{
    // Checking for old EC.
    nx::utils::concurrent::run(
        Ec2ThreadPool::instance(),
        [this, ecUrl, handler, reqId]()
    {
        using namespace std::placeholders;
        return connectToOldEC(
            ecUrl,
            [reqId, handler](
                ErrorCode errorCode, const QnConnectionInfo& oldECConnectionInfo)
        {
            if (errorCode == ErrorCode::ok
                && oldECConnectionInfo.version >= QnSoftwareVersion(2, 3, 0))
            {
                // Somehow connected to 2.3 server with old ec connection. Returning
                // error, since could not connect to ec 2.3 during normal connect.
                handler->done(
                    reqId,
                    ErrorCode::ioError,
                    AbstractECConnectionPtr());
            }
            else
            {
                handler->done(
                    reqId,
                    errorCode,
                    errorCode == ErrorCode::ok
                    ? std::make_shared<OldEcConnection>(oldECConnectionInfo)
                    : AbstractECConnectionPtr());
            }
        });
    });
}

const char oldEcConnectPath[] = "/api/connect/?format=pb&guid&ping=1";

static bool parseOldECConnectionInfo(
    const QByteArray& oldECConnectResponse, QnConnectionInfo* const connectionInfo)
{
    static const char PROTOBUF_FIELD_TYPE_STRING = 0x0a;

    if (oldECConnectResponse.isEmpty())
        return false;

    const char* data = oldECConnectResponse.data();
    const char* dataEnd = oldECConnectResponse.data() + oldECConnectResponse.size();
    if (data + 2 >= dataEnd)
        return false;
    if (*data != PROTOBUF_FIELD_TYPE_STRING)
        return false;
    ++data;
    const int fieldLen = *data;
    ++data;
    if (data + fieldLen >= dataEnd)
        return false;
    connectionInfo->version = QnSoftwareVersion(QByteArray::fromRawData(data, fieldLen));
    return true;
}

template<class Handler>
void RemoteConnectionFactory::connectToOldEC(const nx::utils::Url& ecUrl, Handler completionFunc)
{
    nx::utils::Url httpsEcUrl = ecUrl; // < Old EC supports only https.
    httpsEcUrl.setScheme(lit("https"));

    QAuthenticator auth;
    auth.setUser(httpsEcUrl.userName());
    auth.setPassword(httpsEcUrl.password());
    CLSimpleHTTPClient simpleHttpClient(httpsEcUrl, 3000, auth);
    const CLHttpStatus statusCode = simpleHttpClient.doGET(
        QByteArray::fromRawData(oldEcConnectPath, sizeof(oldEcConnectPath)));
    switch (statusCode)
    {
        case CL_HTTP_SUCCESS:
        {
            // Reading mesasge body.
            QByteArray oldECResponse;
            simpleHttpClient.readAll(oldECResponse);
            QnConnectionInfo oldECConnectionInfo;
            oldECConnectionInfo.ecUrl = httpsEcUrl;
            if (parseOldECConnectionInfo(oldECResponse, &oldECConnectionInfo))
            {
                if (oldECConnectionInfo.version >= QnSoftwareVersion(2, 3))
                {
                    // Ignoring response from 2.3+ server received using compatibility response.
                    completionFunc(ErrorCode::ioError, QnConnectionInfo());
                }
                else
                {
                    completionFunc(ErrorCode::ok, oldECConnectionInfo);
                }
            }
            else
            {
                completionFunc(ErrorCode::badResponse, oldECConnectionInfo);
            }
            break;
        }

        case CL_HTTP_AUTH_REQUIRED:
            completionFunc(ErrorCode::unauthorized, QnConnectionInfo());
            break;

        case CL_HTTP_FORBIDDEN:
            completionFunc(ErrorCode::forbidden, QnConnectionInfo());
            break;

        default:
            completionFunc(ErrorCode::ioError, QnConnectionInfo());
            break;
    }

    QnMutexLocker lk(&m_mutex);
    --m_runningRequests;
}

void RemoteConnectionFactory::remoteConnectionFinished(
    int reqId,
    ErrorCode errorCode,
    const QnConnectionInfo& connectionInfo,
    const nx::utils::Url& ecUrl,
    impl::ConnectHandlerPtr handler)
{
    NX_LOG(QnLog::EC2_TRAN_LOG, lit(
        "RemoteConnectionFactory::remoteConnectionFinished. errorCode = %1, ecUrl = %2")
        .arg((int)errorCode).arg(ecUrl.toString(QUrl::RemovePassword)), cl_logDEBUG2);

    // TODO: #ak async ssl is working now, make async request to old ec here

    switch (errorCode)
    {
        case ec2::ErrorCode::ok:
        case ec2::ErrorCode::unauthorized:
        case ec2::ErrorCode::forbidden:
        case ec2::ErrorCode::ldap_temporary_unauthorized:
        case ec2::ErrorCode::cloud_temporary_unauthorized:
        case ec2::ErrorCode::disabled_user_unauthorized:
            break;

        default:
            tryConnectToOldEC(ecUrl, handler, reqId);
            return;
    }

    QnConnectionInfo connectionInfoCopy(connectionInfo);
    connectionInfoCopy.ecUrl = ecUrl;
    connectionInfoCopy.ecUrl.setScheme(
        connectionInfoCopy.allowSslConnections ? lit("https") : lit("http"));
    connectionInfoCopy.ecUrl.setQuery(QUrlQuery()); /*< Cleanup 'format' parameter. */
    if (nx::network::SocketGlobals::addressResolver().isCloudHostName(ecUrl.host()))
    {
        const auto fullHost =
            connectionInfo.serverId().toSimpleString() + L'.' + connectionInfo.cloudSystemId;
        NX_EXPECT(ecUrl.host() == connectionInfo.cloudSystemId
            || ecUrl.host() == fullHost, "Unexpected cloud host!");
        connectionInfoCopy.ecUrl.setHost(fullHost);
    }

    NX_LOG(QnLog::EC2_TRAN_LOG, lit(
        "RemoteConnectionFactory::remoteConnectionFinished (2). errorCode = %1, ecUrl = %2")
        .arg((int)errorCode).arg(connectionInfoCopy.ecUrl.toString(QUrl::RemovePassword)), cl_logDEBUG2);

    AbstractECConnectionPtr connection(new RemoteEC2Connection(
        m_peerType,
        this,
        connectionInfo.serverId(),
        std::make_shared<FixedUrlClientQueryProcessor>(
            m_remoteQueryProcessor.get(),
            connectionInfoCopy.ecUrl),
        connectionInfoCopy));
    handler->done(reqId, errorCode, connection);

    QnMutexLocker lk(&m_mutex);
    --m_runningRequests;
}

void RemoteConnectionFactory::remoteTestConnectionFinished(
    int reqId,
    ErrorCode errorCode,
    const QnConnectionInfo& connectionInfo,
    const nx::utils::Url& ecUrl,
    impl::TestConnectionHandlerPtr handler)
{
    if (errorCode == ErrorCode::ok
        || errorCode == ErrorCode::unauthorized
        || errorCode == ErrorCode::forbidden
        || errorCode == ErrorCode::ldap_temporary_unauthorized
        || errorCode == ErrorCode::cloud_temporary_unauthorized
        || errorCode == ErrorCode::disabled_user_unauthorized)
    {
        handler->done(reqId, errorCode, connectionInfo);
        QnMutexLocker lk(&m_mutex);
        --m_runningRequests;
        return;
    }

    // Checking for old EC.
    nx::utils::concurrent::run(
        Ec2ThreadPool::instance(),
        [this, ecUrl, handler, reqId]()
        {
            using namespace std::placeholders;
            connectToOldEC(
                ecUrl, std::bind(&impl::TestConnectionHandler::done, handler, reqId, _1, _2));
        }
    );
}

ErrorCode RemoteConnectionFactory::fillConnectionInfo(
    const nx::vms::api::ConnectionData& loginInfo,
    QnConnectionInfo* const connectionInfo,
    nx::network::http::Response* response)
{
    connectionInfo->version = qnStaticCommon->engineVersion();
    connectionInfo->brand = qnStaticCommon->brand();
    connectionInfo->customization = qnStaticCommon->customization();
    connectionInfo->systemName = commonModule()->globalSettings()->systemName();
    connectionInfo->ecsGuid = commonModule()->moduleGUID().toString();
    connectionInfo->cloudSystemId = commonModule()->globalSettings()->cloudSystemId();
    connectionInfo->localSystemId = commonModule()->globalSettings()->localSystemId();
    #if defined(__arm__) || defined(__aarch64__)
        connectionInfo->box = QnAppInfo::armBox();
    #endif
    connectionInfo->allowSslConnections = m_sslEnabled;
    connectionInfo->nxClusterProtoVersion = nx_ec::EC2_PROTO_VERSION;
    connectionInfo->ecDbReadOnly = m_settingsInstance.dbReadOnly();
    connectionInfo->newSystem = commonModule()->globalSettings()->isNewSystem();
    connectionInfo->p2pMode = m_p2pMode;
    if (response)
    {
        connectionInfo->effectiveUserName =
            nx::network::http::getHeaderValue(response->headers, Qn::EFFECTIVE_USER_NAME_HEADER_NAME);
    }

    #ifdef ENABLE_EXTENDED_STATISTICS
        if (!loginInfo.clientInfo.id.isNull())
        {
            auto clientInfo = loginInfo.clientInfo;
            clientInfo.parentId = commonModule()->moduleGUID();

            nx::vms::api::ClientInfoDataList infoList;
            auto result = dbManager(Qn::kSystemAccess).doQuery(clientInfo.id, infoList);
            if (result != ErrorCode::ok)
                return result;

            if (infoList.size() > 0
                && QJson::serialized(clientInfo) == QJson::serialized(infoList.front()))
            {
                NX_LOG(lit("RemoteConnectionFactory: New client had already been registered with the same params"),
                    cl_logDEBUG2);
                return ErrorCode::ok;
            }

            m_serverQueryProcessor.getAccess(Qn::kSystemAccess).processUpdateAsync(
                ApiCommand::saveClientInfo, clientInfo,
                [&](ErrorCode result)
                {
                    if (result == ErrorCode::ok)
                    {
                        NX_LOG(lit("RemoteConnectionFactory: New client has been registered"),
                            cl_logINFO);
                    }
                    else
                    {
                        NX_LOG(lit("RemoteConnectionFactory: New client transaction has failed %1")
                            .arg(toString(result)), cl_logERROR);
                    }
                });
        }
    #else
        nx::utils::unused(loginInfo);
    #endif

    return ErrorCode::ok;
}

int RemoteConnectionFactory::testDirectConnection(
    const nx::utils::Url& /*addr*/,
    impl::TestConnectionHandlerPtr handler)
{
    const int reqId = generateRequestID();
    QnConnectionInfo connectionInfo;
    fillConnectionInfo(nx::vms::api::ConnectionData(), &connectionInfo);
    nx::utils::concurrent::run(
        Ec2ThreadPool::instance(),
        std::bind(
            &impl::TestConnectionHandler::done,
            handler,
            reqId,
            ErrorCode::ok,
            connectionInfo));
    return reqId;
}

int RemoteConnectionFactory::testRemoteConnection(
    const nx::utils::Url& addr,
    impl::TestConnectionHandlerPtr handler)
{
    const int reqId = generateRequestID();

    {
        QnMutexLocker lk(&m_mutex);
        if (m_terminated)
            return INVALID_REQ_ID;
        ++m_runningRequests;
    }

    nx::vms::api::ConnectionData loginInfo;
    loginInfo.login = addr.userName();
    loginInfo.passwordHash = nx::network::http::calcHa1(
        loginInfo.login.toLower(), nx::network::AppInfo::realm(), addr.password());
    auto func =
        [this, reqId, addr, handler](ErrorCode errorCode, const QnConnectionInfo& connectionInfo)
        {
            auto infoWithUrl = connectionInfo;
            infoWithUrl.ecUrl = addr;
            infoWithUrl.ecUrl.setQuery(QUrlQuery()); /*< Cleanup 'format' parameter. */
            remoteTestConnectionFinished(reqId, errorCode, infoWithUrl, addr, handler);
        };
    m_remoteQueryProcessor->processQueryAsync<std::nullptr_t, QnConnectionInfo>(
        addr, ApiCommand::testConnection, std::nullptr_t(), func);
    return reqId;
}

TransactionMessageBusAdapter* RemoteConnectionFactory::messageBus() const
{
    return m_bus.get();
}

nx::time_sync::TimeSyncManager* RemoteConnectionFactory::timeSyncManager() const
{
    return m_timeSynchronizationManager.get();
}

} // namespace ec2
