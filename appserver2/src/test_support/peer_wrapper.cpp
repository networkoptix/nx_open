#include "peer_wrapper.h"

namespace ec2 {
namespace test {

//namespace {

// TODO: #ak Get rid of this class. ec2GetTransactionLog should be in MediaServerClient. 
// Though, it requires some refactoring: moving ec2::ApiTransactionDataList out of appserver2.
class MediaServerClientEx:
    public MediaServerClient
{
    using base_type = MediaServerClient;

public:
    MediaServerClientEx(const nx::utils::Url& baseRequestUrl):
        base_type(baseRequestUrl)
    {
    }

    void ec2GetTransactionLog(
        std::function<void(ec2::ErrorCode, ec2::ApiTransactionDataList)> completionHandler)
    {
        performAsyncEc2Call("ec2/getTransactionLog", std::move(completionHandler));
    }

    ec2::ErrorCode ec2GetTransactionLog(ec2::ApiTransactionDataList* result)
    {
        using Ec2GetTransactionLogAsyncFuncPointer =
            void(MediaServerClientEx::*)(
                std::function<void(ec2::ErrorCode, ec2::ApiTransactionDataList)>);

        return syncCallWrapper(
            this,
            static_cast<Ec2GetTransactionLogAsyncFuncPointer>(
                &MediaServerClientEx::ec2GetTransactionLog),
            result);
    }
};

//} // namespace

//-------------------------------------------------------------------------------------------------

PeerWrapper::PeerWrapper(const QString& dataDir):
    m_dataDir(dataDir)
{
    m_ownerCredentials.username = "admin";
    m_ownerCredentials.authToken = nx_http::PasswordAuthToken("admin");
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

    const QString password = nx::utils::generateRandomName(7);

    SetupLocalSystemData request;
    request.systemName = nx::utils::generateRandomName(7);
    request.password = password;

    if (mediaServerClient->setupLocalSystem(std::move(request)).error !=
        QnJsonRestResult::NoError)
    {
        return false;
    }

    m_ownerCredentials.authToken = nx_http::PasswordAuthToken(password);
    return true;
}

QnRestResult::Error PeerWrapper::mergeTo(const PeerWrapper& remotePeer)
{
    MergeSystemData mergeSystemData;
    mergeSystemData.takeRemoteSettings = true;
    mergeSystemData.mergeOneServer = false;
    mergeSystemData.ignoreIncompatible = false;
    const auto nonce = nx::utils::generateRandomName(7);
    mergeSystemData.getKey = buildAuthKey("/api/mergeSystems", m_ownerCredentials, nonce);
    mergeSystemData.postKey = buildAuthKey("/api/mergeSystems", m_ownerCredentials, nonce);
    mergeSystemData.url = nx::network::url::Builder()
        .setScheme(nx_http::kUrlSchemeName)
        .setEndpoint(remotePeer.endpoint()).toString();

    auto mediaServerClient = prepareMediaServerClient();
    return mediaServerClient->mergeSystems(mergeSystemData).error;
}

ec2::ErrorCode PeerWrapper::getTransactionLog(ec2::ApiTransactionDataList* result) const
{
    auto mediaServerClient = prepareMediaServerClient();
    return mediaServerClient->ec2GetTransactionLog(result);
}

SocketAddress PeerWrapper::endpoint() const
{
    return m_process.moduleInstance()->endpoint();
}

bool PeerWrapper::areAllPeersHaveSameTransactionLog(
    const std::vector<std::unique_ptr<PeerWrapper>>& peers)
{
    std::vector<::ec2::ApiTransactionDataList> transactionLogs;
    for (const auto& server : peers)
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
    for (const auto& transactionLog : transactionLogs)
    {
        if (prevTransactionLog)
        {
            if (*prevTransactionLog != transactionLog)
                allLogsAreEqual = false;
        }

        prevTransactionLog = &transactionLog;
    }

    return allLogsAreEqual;
}

std::unique_ptr<MediaServerClientEx> PeerWrapper::prepareMediaServerClient() const
{
    auto mediaServerClient = std::make_unique<MediaServerClientEx>(
        nx::network::url::Builder().setScheme(nx_http::kUrlSchemeName)
        .setEndpoint(m_process.moduleInstance()->endpoint()));
    mediaServerClient->setUserCredentials(m_ownerCredentials);
    return mediaServerClient;
}

QString PeerWrapper::buildAuthKey(
    const nx::String& url,
    const nx_http::Credentials& credentials,
    const nx::String& nonce)
{
    const auto ha1 = nx_http::calcHa1(
        credentials.username,
        nx::network::AppInfo::realm(),
        credentials.authToken.value);
    const auto ha2 = nx_http::calcHa2(nx_http::Method::get, url);
    const auto response = nx_http::calcResponse(ha1, nonce, ha2);
    return lm("%1:%2:%3").args(credentials.username, nonce, response).toUtf8().toBase64();
}

} // namespace test
} // namespace ec2
