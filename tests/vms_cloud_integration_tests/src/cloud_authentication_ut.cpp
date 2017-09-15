#include <gtest/gtest.h>

#include <nx/network/http/auth_tools.h>
#include <nx/network/http/http_client.h>
#include <nx/network/http/server/http_server_connection.h>
#include <nx/network/system_socket.h>
#include <nx/utils/literal.h>
#include <nx/utils/random.h>

#include <nx/utils/scope_guard.h>
#include <nx/utils/thread/long_runnable.h>
#include <nx/utils/sync_call.h>
#include <utils/common/util.h>

#include <media_server/media_server_module.h>
#include <media_server/serverutil.h>

#include "mediaserver_cloud_integration_test_setup.h"
#include <test_support/mediaserver_launcher.h>

using namespace nx::cdb;

class CloudAuthentication:
    public MediaServerCloudIntegrationTest
{
public:
    CloudAuthentication()
    {
        connectSystemToCloud();
    }

protected:
    void givenCloudNonce()
    {
        getCloudNonce(&m_cloudNonceData);
    }

    void givenTemporaryCloudCredentials()
    {
        nx::cdb::api::TemporaryCredentialsParams temporaryCredentialsParams;
        temporaryCredentialsParams.type = "long";
        nx::cdb::api::TemporaryCredentials temporaryCredentials;
        ASSERT_EQ(
            nx::cdb::api::ResultCode::ok,
            cdb()->createTemporaryCredentials(
                accountEmail(), accountPassword(),
                temporaryCredentialsParams, &temporaryCredentials));

        m_customCredentials = 
            std::make_pair(temporaryCredentials.login, temporaryCredentials.password);
    }

    void whenIssuedHttpRequestSignedWithThatNonce()
    {
        nx_http::Message requestMsg = prepareRequest(lit("/ec2/getUsers"), m_cloudNonceData);

        //issuing request to mediaserver using that nonce
        auto tcpSocket = std::make_unique<nx::network::TCPSocket>(AF_INET);
        ASSERT_TRUE(tcpSocket->connect(mediaServerEndpoint(), 3000));
        ASSERT_TRUE(tcpSocket->setNonBlockingMode(true));
        auto httpMsgPipeline = std::make_unique<nx_http::deprecated::AsyncMessagePipeline>(
            nullptr,
            std::move(tcpSocket));
        httpMsgPipeline->startReadingConnection();

        auto httpMsgPipelineGuard = makeScopeGuard(
            [&httpMsgPipeline]() { httpMsgPipeline->pleaseStopSync(); });

        nx::utils::promise<nx_http::Message> responseReceivedPromise;
        httpMsgPipeline->setMessageHandler(
            [&responseReceivedPromise](nx_http::Message msg)
            {
                responseReceivedPromise.set_value(std::move(msg));
            });
        httpMsgPipeline->sendMessage(std::move(requestMsg), [](SystemError::ErrorCode) {});

        auto responseReceivedFuture = responseReceivedPromise.get_future();
        m_responseMessage = std::move(responseReceivedFuture.get());
    }

    void thenRequestShouldBeFulfilled()
    {
        ASSERT_EQ(nx_http::MessageType::response, m_responseMessage.type);
        ASSERT_EQ(nx_http::StatusCode::ok, m_responseMessage.response->statusLine.statusCode);
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
        nx_http::HttpClient httpClient;
        httpClient.setUserName(QString::fromStdString(userName).toLower());
        httpClient.setUserPassword(QString::fromStdString(password));
        if (!httpClient.doGet(lit("http://%1/ec2/getUsers").arg(mediaServerEndpoint().toString())))
            return false;
        if (httpClient.response() == nullptr)
            return false;
        if (httpClient.response()->statusLine.statusCode != nx_http::StatusCode::ok)
            return false;
        return true;
    }

private:
    api::NonceData m_cloudNonceData;
    nx_http::Message m_responseMessage;
    boost::optional<std::pair<std::string, std::string>> m_customCredentials;

    void getCloudNonce(api::NonceData* nonceData)
    {
        typedef void(nx::cdb::api::AuthProvider::*GetCdbNonceType)
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

    nx_http::Message prepareRequest(
        const QString& requestPath,
        const api::NonceData& nonceData)
    {
        nx_http::Message requestMsg(nx_http::MessageType::request);
        requestMsg.request->requestLine.url = requestPath;
        requestMsg.request->requestLine.method = nx_http::Method::get;
        requestMsg.request->requestLine.version = nx_http::http_1_1;

        addAuthorizationHeader(nonceData, requestMsg.request);
        return requestMsg;
    }

    void addAuthorizationHeader(
        const api::NonceData& nonceData,
        nx_http::Request* const request)
    {
        nx_http::header::WWWAuthenticate wwwAuthenticate;
        wwwAuthenticate.authScheme = nx_http::header::AuthScheme::digest;
        wwwAuthenticate.params.insert("nonce", nonceData.nonce.c_str());
        wwwAuthenticate.params.insert("realm", "VMS");

        nx_http::header::DigestAuthorization digestAuthorization;

        nx_http::calcDigestResponse(
            request->requestLine.method,
            accountEmail().c_str(),
            nx_http::StringType(accountPassword().c_str()),
            boost::none,
            request->requestLine.url.toString().toUtf8(),
            wwwAuthenticate,
            &digestAuthorization);

        request->headers.emplace(
            nx_http::header::Authorization::NAME,
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

//-------------------------------------------------------------------------------------------------
// CloudAuthenticationInviteUser

class CloudAuthenticationInviteUser:
    public CloudAuthentication
{
public:

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
        {
            std::this_thread::sleep_for(std::chrono::seconds(1));
        }
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
