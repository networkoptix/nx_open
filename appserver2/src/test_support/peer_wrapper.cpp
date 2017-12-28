#include "peer_wrapper.h"

#include <api/auth_util.h>
#include <rest/request_params.h>

#include <transaction/message_bus_adapter.h>

namespace ec2 {
namespace test {

MediaServerClientEx::MediaServerClientEx(const nx::utils::Url& baseRequestUrl):
    base_type(baseRequestUrl)
{
}

void MediaServerClientEx::ec2GetTransactionLog(
    const ApiTranLogFilter& filter,
    std::function<void(ec2::ErrorCode, ec2::ApiTransactionDataList)> completionHandler)
{
    QString requestPath("ec2/getTransactionLog");
    QUrlQuery urlQuery;
    ::ec2::toUrlParams(filter, &urlQuery);
    if (!urlQuery.isEmpty())
        requestPath += lm("?%1").args(urlQuery.toString());

    performAsyncEc2Call(requestPath.toStdString(), std::move(completionHandler));
}

ec2::ErrorCode MediaServerClientEx::ec2GetTransactionLog(
    const ApiTranLogFilter& filter,
    ec2::ApiTransactionDataList* result)
{
    using Ec2GetTransactionLogAsyncFuncPointer =
        void(MediaServerClientEx::*)(
            const ApiTranLogFilter&,
            std::function<void(ec2::ErrorCode, ec2::ApiTransactionDataList)>);

    return syncCallWrapper(
        this,
        static_cast<Ec2GetTransactionLogAsyncFuncPointer>(
            &MediaServerClientEx::ec2GetTransactionLog),
        filter,
        result);
}

//-------------------------------------------------------------------------------------------------

PeerWrapper::PeerWrapper(const QString& dataDir):
    m_dataDir(dataDir)
{
    m_ownerCredentials.username = "admin";
    m_ownerCredentials.authToken = nx::network::http::PasswordAuthToken("admin");
}

void PeerWrapper::addSetting(const std::string& name, const std::string& value)
{
    m_process.addArg(name.c_str(), value.c_str());
}

bool PeerWrapper::startAndWaitUntilStarted()
{
    const QString dbFileArg = lm("--dbFile=%1/db.sqlite").args(m_dataDir);
    m_process.addArg(dbFileArg.toStdString().c_str());
    return m_process.startAndWaitUntilStarted();
}

bool PeerWrapper::configureAsLocalSystem()
{
    auto mediaServerClient = prepareMediaServerClient();

    const auto password = nx::utils::generateRandomName(7);

    SetupLocalSystemData request;
    request.systemName = nx::utils::generateRandomName(7);
    request.password = password;

    if (mediaServerClient->setupLocalSystem(std::move(request)).error !=
        QnJsonRestResult::NoError)
    {
        return false;
    }

    m_ownerCredentials.authToken = nx::network::http::PasswordAuthToken(password);
    return true;
}

bool PeerWrapper::saveCloudSystemCredentials(
    const std::string& systemId,
    const std::string& authKey,
    const std::string& ownerAccountEmail)
{
    auto mserverClient = prepareMediaServerClient();

    CloudCredentialsData cloudData;
    cloudData.cloudSystemID = QString::fromStdString(systemId);
    cloudData.cloudAuthKey = QString::fromStdString(authKey);
    cloudData.cloudAccountName = QString::fromStdString(ownerAccountEmail);
    if (mserverClient->saveCloudSystemCredentials(std::move(cloudData)).error !=
            QnJsonRestResult::NoError)
    {
        return false;
    }

    m_cloudCredentials.systemId = systemId.c_str();
    m_cloudCredentials.key = authKey.c_str();
    m_cloudCredentials.serverId = id().toSimpleByteArray();

    return true;
}

QnRestResult::Error PeerWrapper::mergeTo(const PeerWrapper& remotePeer)
{
    MergeSystemData mergeSystemData;
    mergeSystemData.takeRemoteSettings = true;
    mergeSystemData.mergeOneServer = false;
    mergeSystemData.ignoreIncompatible = false;

    AuthKey authKey;
    authKey.username = m_ownerCredentials.username.toUtf8();
    authKey.nonce = nx::utils::generateRandomName(7);

    authKey.calcResponse(
        m_ownerCredentials.authToken,
        nx::network::http::Method::get,
        "/api/mergeSystems");
    mergeSystemData.getKey = authKey.toString();

    authKey.calcResponse(
        m_ownerCredentials.authToken,
        nx::network::http::Method::post,
        "/api/mergeSystems");
    mergeSystemData.postKey = authKey.toString();

    mergeSystemData.url = nx::network::url::Builder()
        .setScheme(nx::network::http::kUrlSchemeName)
        .setEndpoint(remotePeer.endpoint()).toString();

    auto mediaServerClient = prepareMediaServerClient();
    return mediaServerClient->mergeSystems(mergeSystemData).error;
}

ec2::ErrorCode PeerWrapper::getTransactionLog(ec2::ApiTransactionDataList* result) const
{
    ApiTranLogFilter filter;
    filter.cloudOnly = false;

    auto mediaServerClient = prepareMediaServerClient();
    return mediaServerClient->ec2GetTransactionLog(filter, result);
}

QnUuid PeerWrapper::id() const
{
    return m_process.moduleInstance()->impl()->commonModule()->moduleGUID();
}

nx::network::SocketAddress PeerWrapper::endpoint() const
{
    return m_process.moduleInstance()->endpoint();
}

ec2::AbstractECConnection* PeerWrapper::ecConnection()
{
    return m_process.moduleInstance()->ecConnection();
}

nx::hpm::api::SystemCredentials PeerWrapper::getCloudCredentials() const
{
    return m_cloudCredentials;
}

std::unique_ptr<MediaServerClientEx> PeerWrapper::mediaServerClient() const
{
    return prepareMediaServerClient();
}

nx::utils::test::ModuleLauncher<Appserver2ProcessPublic>& PeerWrapper::process()
{
    return m_process;
}

bool PeerWrapper::areAllPeersHaveSameTransactionLog(
    const std::vector<std::unique_ptr<PeerWrapper>>& peers)
{
    std::vector<::ec2::ApiTransactionDataList> transactionLogs;
    for (const auto& server: peers)
    {
        ::ec2::ApiTransactionDataList transactionLog;
        const auto ec2ErrorCode = server->getTransactionLog(&transactionLog);
        if (ec2ErrorCode != ::ec2::ErrorCode::ok)
        {
            throw std::runtime_error(
                lm("getTransactionLog request failed. %1")
                .args(::ec2::toString(ec2ErrorCode)).toStdString());
        }
        transactionLogs.push_back(std::move(transactionLog));
    }

    const ::ec2::ApiTransactionDataList* prevTransactionLog = nullptr;
    bool allLogsAreEqual = true;
    for (const auto& transactionLog: transactionLogs)
    {
        //std::cout << "=========================\n";
        //std::cout << QJson::serialized(transactionLog).toStdString() << "\n";
        //std::cout << "=========================\n";

        if (prevTransactionLog)
        {
            if (*prevTransactionLog != transactionLog)
                allLogsAreEqual = false;
        }

        prevTransactionLog = &transactionLog;
    }

    return allLogsAreEqual;
}

bool PeerWrapper::arePeersInterconnected(
    const std::vector<std::unique_ptr<PeerWrapper>>& peers)
{
    // For now just checking that each peer is connected to every other.

    std::vector<QnUuid> peerIds;
    for (const auto& peer: peers)
        peerIds.push_back(peer->id());

    for (const auto& peer: peers)
    {
        const auto connectedPeers =
            peer->m_process.moduleInstance()->impl()->commonModule()->
                ec2Connection()->messageBus()->directlyConnectedServerPeers();

        for (const auto& peerId: peerIds)
        {
            if (peerId == peer->id())
                continue;
            if (std::find(connectedPeers.begin(), connectedPeers.end(), peerId) ==
                    connectedPeers.end())
            {
                return false;
            }
        }
    }

    return true;
}

std::unique_ptr<MediaServerClientEx> PeerWrapper::prepareMediaServerClient() const
{
    auto mediaServerClient = std::make_unique<MediaServerClientEx>(
        nx::network::url::Builder().setScheme(nx::network::http::kUrlSchemeName)
        .setEndpoint(m_process.moduleInstance()->endpoint()));
    mediaServerClient->setUserCredentials(m_ownerCredentials);
    mediaServerClient->setRequestTimeout(std::chrono::minutes(1));
    return mediaServerClient;
}

} // namespace test
} // namespace ec2
