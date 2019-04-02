#include <gtest/gtest.h>

#include <nx/clusterdb/engine/connection_manager.h>
#include <nx/clusterdb/engine/synchronization_engine.h>
#include <nx/network/http/auth_tools.h>
#include <nx/network/http/http_client.h>
#include <nx/network/http/server/http_server_connection.h>
#include <nx/network/system_socket.h>
#include <nx/utils/literal.h>
#include <nx/utils/random.h>
#include <nx/utils/scope_guard.h>
#include <nx/utils/thread/long_runnable.h>
#include <nx/utils/sync_call.h>

#include <nx/clusterdb/engine/service/service.h>
#include <nx/clusterdb/engine/transport/connector_factory.h>
#include <nx/clusterdb/engine/transport/p2p_websocket/connector.h>

#include <nx_ec/ec_proto_version.h>
#include <media_server/media_server_module.h>
#include <media_server/serverutil.h>
#include <utils/common/util.h>
#include <test_support/mediaserver_launcher.h>

#include "mediaserver_cloud_integration_test_setup.h"

using namespace nx::cloud::db;

class CloudAuthentication:
    public MediaServerCloudIntegrationTest
{
    using base_type = MediaServerCloudIntegrationTest;

public:
    CloudAuthentication()
    {
    }

protected:
    virtual void SetUp() override
    {
        base_type::SetUp();

        connectSystemToCloud();
    }

    void givenCloudNonce()
    {
        getCloudNonce(&m_cloudNonceData);
    }

    void givenTemporaryCloudCredentials()
    {
        nx::cloud::db::api::TemporaryCredentialsParams temporaryCredentialsParams;
        temporaryCredentialsParams.type = "long";
        nx::cloud::db::api::TemporaryCredentials temporaryCredentials;
        ASSERT_EQ(
            nx::cloud::db::api::ResultCode::ok,
            cdb()->createTemporaryCredentials(
                accountEmail(), accountPassword(),
                temporaryCredentialsParams, &temporaryCredentials));

        m_customCredentials =
            std::make_pair(temporaryCredentials.login, temporaryCredentials.password);
    }

    void whenIssuedHttpRequestSignedWithThatNonce()
    {
        nx::network::http::Message requestMsg = prepareRequest(lit("/ec2/getUsers"), m_cloudNonceData);

        //issuing request to mediaserver using that nonce
        auto tcpSocket = std::make_unique<nx::network::TCPSocket>(AF_INET);
        ASSERT_TRUE(tcpSocket->connect(mediaServerEndpoint(), nx::network::kNoTimeout));
        ASSERT_TRUE(tcpSocket->setNonBlockingMode(true));
        auto httpMsgPipeline = std::make_unique<nx::network::http::deprecated::AsyncMessagePipeline>(
            std::move(tcpSocket));
        httpMsgPipeline->startReadingConnection();

        auto httpMsgPipelineGuard = nx::utils::makeScopeGuard(
            [&httpMsgPipeline]() { httpMsgPipeline->pleaseStopSync(); });

        nx::utils::promise<nx::network::http::Message> responseReceivedPromise;
        httpMsgPipeline->setMessageHandler(
            [&responseReceivedPromise](nx::network::http::Message msg)
            {
                responseReceivedPromise.set_value(std::move(msg));
            });
        httpMsgPipeline->sendMessage(std::move(requestMsg), [](SystemError::ErrorCode) {});

        auto responseReceivedFuture = responseReceivedPromise.get_future();
        m_responseMessage = std::move(responseReceivedFuture.get());
    }

    void thenRequestShouldBeFulfilled()
    {
        ASSERT_EQ(nx::network::http::MessageType::response, m_responseMessage.type);
        ASSERT_EQ(nx::network::http::StatusCode::ok, m_responseMessage.response->statusLine.statusCode);
    }

    void assertServerAuthorizesCloudUserCredentials()
    {
        auto credentials = m_customCredentials
            ? *m_customCredentials
            : std::make_pair(accountEmail(), accountPassword());

        ASSERT_TRUE(userRequestIsAuthorizedByServer(credentials.first, credentials.second));
    }

    bool userRequestIsAuthorizedByServer(std::string userName, std::string password)
    {
        nx::network::http::HttpClient httpClient;
        httpClient.setUserName(QString::fromStdString(userName).toLower());
        httpClient.setUserPassword(QString::fromStdString(password));
        if (!httpClient.doGet(lit("http://%1/ec2/getUsers").arg(mediaServerEndpoint().toString())))
            return false;
        if (httpClient.response() == nullptr)
            return false;
        if (httpClient.response()->statusLine.statusCode != nx::network::http::StatusCode::ok)
            return false;
        return true;
    }

private:
    api::NonceData m_cloudNonceData;
    nx::network::http::Message m_responseMessage;
    boost::optional<std::pair<std::string, std::string>> m_customCredentials;

    void getCloudNonce(api::NonceData* nonceData)
    {
        typedef void(nx::cloud::db::api::AuthProvider::*GetCdbNonceType)
            (const std::string&, std::function<void(api::ResultCode, api::NonceData)>);

        auto cdbConnection =
            cdb()->connectionFactory()->createConnection(
                accountEmail(),
                accountPassword());
        api::ResultCode resultCode = api::ResultCode::ok;
        std::tie(resultCode, *nonceData) =
            makeSyncCall<api::ResultCode, api::NonceData>(
                std::bind(
                    static_cast<GetCdbNonceType>(&api::AuthProvider::getCdbNonce),
                    cdbConnection->authProvider(),
                    cloudSystem().id,
                    std::placeholders::_1));
        ASSERT_EQ(api::ResultCode::ok, resultCode);
    }

    nx::network::http::Message prepareRequest(
        const QString& requestPath,
        const api::NonceData& nonceData)
    {
        nx::network::http::Message requestMsg(nx::network::http::MessageType::request);
        requestMsg.request->requestLine.url = requestPath;
        requestMsg.request->requestLine.method = nx::network::http::Method::get;
        requestMsg.request->requestLine.version = nx::network::http::http_1_1;

        addAuthorizationHeader(nonceData, requestMsg.request);
        return requestMsg;
    }

    void addAuthorizationHeader(
        const api::NonceData& nonceData,
        nx::network::http::Request* const request)
    {
        nx::network::http::header::WWWAuthenticate wwwAuthenticate;
        wwwAuthenticate.authScheme = nx::network::http::header::AuthScheme::digest;
        wwwAuthenticate.params.emplace("nonce", nonceData.nonce.c_str());
        wwwAuthenticate.params.emplace("realm", "VMS");

        nx::network::http::header::DigestAuthorization digestAuthorization;

        nx::network::http::calcDigestResponse(
            request->requestLine.method,
            accountEmail().c_str(),
            nx::network::http::StringType(accountPassword().c_str()),
            boost::none,
            request->requestLine.url.toString().toUtf8(),
            wwwAuthenticate,
            &digestAuthorization);

        request->headers.emplace(
            nx::network::http::header::Authorization::NAME,
            digestAuthorization.serialized());
    }
};

TEST_P(CloudAuthentication, authorize_api_request_with_cloud_credentials)
{
    assertServerAuthorizesCloudUserCredentials();
}

TEST_P(CloudAuthentication, one_step_digest_authentication_using_nonce_received_from_cloud)
{
    givenCloudNonce();
    whenIssuedHttpRequestSignedWithThatNonce();
    thenRequestShouldBeFulfilled();
}

TEST_P(CloudAuthentication, authenticating_on_mediaserver_using_temporary_cloud_credentials)
{
    givenTemporaryCloudCredentials();
    assertServerAuthorizesCloudUserCredentials();
}

INSTANTIATE_TEST_CASE_P(P2pMode, CloudAuthentication,
    ::testing::Values(TestParams(false), TestParams(true)
));

//-------------------------------------------------------------------------------------------------
// CloudAuthenticationInviteUser

class CloudAuthenticationInviteUser:
    public CloudAuthentication
{
protected:
    void inviteCloudUser()
    {
        m_inivitedUserEmail = cdb()->generateRandomEmailAddress();
        api::SystemSharing sharing;
        sharing.systemId = cloudSystem().id;
        sharing.accessRole = api::SystemAccessRole::cloudAdmin;
        sharing.accountEmail = m_inivitedUserEmail;
        ASSERT_EQ(
            api::ResultCode::ok,
            cdb()->shareSystem(accountEmail(), accountPassword(), sharing));

        api::AccountData newAccount;
        newAccount.email = m_inivitedUserEmail;
        api::AccountConfirmationCode accountConfirmationCode;
        ASSERT_EQ(
            api::ResultCode::ok,
            cdb()->addAccount(&newAccount, &m_inivitedUserPassword, &accountConfirmationCode));

        std::string resultEmail;
        ASSERT_EQ(
            api::ResultCode::ok,
            cdb()->activateAccount(accountConfirmationCode, &resultEmail));
    }

    void assertInvitedUserIsAuthenticatedSuccessfully()
    {
        while (!userRequestIsAuthorizedByServer(m_inivitedUserEmail, m_inivitedUserPassword))
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }

private:
    std::string m_inivitedUserEmail;
    std::string m_inivitedUserPassword;
};

TEST_P(CloudAuthenticationInviteUser, invited_user_is_authorized_by_mediaserver)
{
    inviteCloudUser();
    assertInvitedUserIsAuthenticatedSuccessfully();
}

INSTANTIATE_TEST_CASE_P(P2pMode, CloudAuthenticationInviteUser,
    ::testing::Values(TestParams(false), TestParams(true)
));

//-------------------------------------------------------------------------------------------------

namespace {

class CloudNode:
    public nx::clusterdb::engine::Service
{
    using base_type = nx::clusterdb::engine::Service;

public:
    CloudNode(
        int argc,
        char** argv)
        :
        base_type("", argc, argv)
    {
        nx::clusterdb::engine::ProtocolVersionRange protocolVersionRange(
            nx_ec::EC2_PROTO_VERSION,
            nx_ec::EC2_PROTO_VERSION);
        setSupportedProtocolRange(protocolVersionRange);
    }
};

} // namespace

class CloudAuthenticationForSynchronization:
    public CloudAuthentication
{
    using base_type = CloudAuthentication;

protected:
    virtual void SetUp() override
    {
        base_type::SetUp();

        const auto dbPath = lm("%1/cloud_node_db.sqlite").args(testDataDir()).toStdString();
        m_cloudNode.addArg("-db/name", dbPath.c_str());
        m_cloudNode.addArg("-p2pDb/clusterId", cloudSystem().id.c_str());
        m_cloudNode.addArg("-api/baseHttpPath", "");

        ASSERT_TRUE(m_cloudNode.startAndWaitUntilStarted());
    }

    void whenEstablishSynchronizationConnectionUsingCloudSystemCredentials()
    {
        auto url = nx::network::url::Builder(mediaServer().apiUrl()).appendPath("ec2/").toUrl();
        url.setUserName(cloudSystem().id.c_str());
        url.setPassword(cloudSystem().authKey.c_str());

        m_connector = std::make_unique<nx::clusterdb::engine::transport::p2p::websocket::Connector>(
            m_cloudNode.moduleInstance()->protocolVersionRange(),
            &m_cloudNode.moduleInstance()->synchronizationEngine().transactionLog(),
            m_cloudNode.moduleInstance()->synchronizationEngine().outgoingCommandFilter(),
            url,
            cloudSystem().id,
            "nodeId");

        m_connector->connect(
            [this](auto&&... args)
            {
                saveConnectCompletion(std::forward<decltype(args)>(args)...);
            });
    }

    void thenConnectionIsEstablished()
    {
        const auto result = m_connectResults.pop();
        ASSERT_TRUE(result.ok());
    }

private:
    nx::utils::test::ModuleLauncher<CloudNode> m_cloudNode;
    nx::utils::SyncQueue<nx::clusterdb::engine::transport::ConnectResultDescriptor> m_connectResults;
    std::unique_ptr<nx::clusterdb::engine::transport::p2p::websocket::Connector> m_connector;

    void saveConnectCompletion(
        nx::clusterdb::engine::transport::ConnectResultDescriptor connectResultDescriptor,
        std::unique_ptr<nx::clusterdb::engine::transport::AbstractConnection> /*connection*/)
    {
        m_connectResults.push(std::move(connectResultDescriptor));
    }
};

TEST_P(CloudAuthenticationForSynchronization, DISABLED_cloud_system_credentials_are_allowed)
{
    whenEstablishSynchronizationConnectionUsingCloudSystemCredentials();

    thenConnectionIsEstablished();
}

INSTANTIATE_TEST_CASE_P(
    P2pMode,
    CloudAuthenticationForSynchronization,
    ::testing::Values(TestParams(true)));
