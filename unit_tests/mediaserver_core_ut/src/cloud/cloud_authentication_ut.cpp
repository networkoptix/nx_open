
#include <gtest/gtest.h>

#include <nx/network/http/auth_tools.h>
#include <nx/network/http/httpclient.h>
#include <nx/network/http/server/http_server_connection.h>
#include <nx/network/system_socket.h>
#include <nx/utils/literal.h>
#include <nx/utils/random.h>
#include <utils/common/sync_call.h>
#include <utils/common/util.h>

#include <media_server/media_server_module.h>
#include <utils/common/long_runnable.h>

#include "media_server/serverutil.h"
#include "mediaserver_launcher.h"
#include "mediaserver_cloud_integration_test_setup.h"


using namespace nx::cdb;

class CloudAuthentication
:
    public MediaServerCloudIntegrationTest,
    public ::testing::Test
{
public:
    std::string accountEmail;
    std::string accountPassword;
    std::string cloudSystemId;
    std::string cloudSystemAuthKey;

    virtual void SetUp() override
    {
        ASSERT_TRUE(startCloudDB());
        ASSERT_TRUE(startMediaServer());

        ASSERT_TRUE(registerRandomCloudAccount(&accountEmail, &accountPassword));
        ASSERT_TRUE(bindSystemToCloud(
            accountEmail, accountPassword,
            &cloudSystemId, &cloudSystemAuthKey));
    }

    void doGeneralTest()
    {
        //const auto cdbConnection = getCdbConnection();

        nx_http::HttpClient httpClient;
        httpClient.setUserName(QString::fromStdString(accountEmail));
        httpClient.setUserPassword(QString::fromStdString(accountPassword));
        ASSERT_TRUE(httpClient.doGet(
            lit("http://%1/ec2/getSettings").arg(mediaServerEndpoint().toString())));
        ASSERT_NE(nullptr, httpClient.response());
        ASSERT_EQ(nx_http::StatusCode::ok, httpClient.response()->statusLine.statusCode);
    }

    void doNonceFromCloudDbTest()
    {
        typedef void(nx::cdb::api::AuthProvider::*GetCdbNonceType)
            (const std::string&, std::function<void(api::ResultCode, api::NonceData)>);

        //fetching nonce from cloud_db
        auto cdbConnection =
            cdb()->connectionFactory()->createConnection(
                accountEmail,
                accountPassword);
        api::ResultCode resultCode = api::ResultCode::ok;
        api::NonceData nonceData;
        std::tie(resultCode, nonceData) =
            makeSyncCall<api::ResultCode, api::NonceData>(
                std::bind(
                    static_cast<GetCdbNonceType>(&api::AuthProvider::getCdbNonce),
                    cdbConnection->authProvider(),
                    cloudSystemId,
                    std::placeholders::_1));
        ASSERT_EQ(api::ResultCode::ok, resultCode);

        //preparing request
        nx_http::Message requestMsg(nx_http::MessageType::request);
        requestMsg.request->requestLine.url = lit("/ec2/getUsers");
        requestMsg.request->requestLine.method = nx_http::Method::GET;
        requestMsg.request->requestLine.version = nx_http::http_1_1;

        nx_http::header::WWWAuthenticate wwwAuthenticate;
        wwwAuthenticate.authScheme = nx_http::header::AuthScheme::digest;
        wwwAuthenticate.params.insert("nonce", nonceData.nonce.c_str());
        wwwAuthenticate.params.insert("realm", "VMS");

        nx_http::header::DigestAuthorization digestAuthorization;

        nx_http::calcDigestResponse(
            requestMsg.request->requestLine.method,
            accountEmail.c_str(),
            nx_http::StringType(accountPassword.c_str()),
            boost::none,
            requestMsg.request->requestLine.url.toString().toUtf8(),
            wwwAuthenticate,
            &digestAuthorization);

        requestMsg.request->headers.emplace(
            nx_http::header::Authorization::NAME,
            digestAuthorization.serialized());

        //issuing request to mediaserver using that nonce
        auto tcpSocket = std::make_unique<nx::network::TCPSocket>(AF_INET);
        ASSERT_TRUE(tcpSocket->connect(mediaServerEndpoint(), 3000));
        ASSERT_TRUE(tcpSocket->setNonBlockingMode(true));
        auto httpMsgPipeline = std::make_unique<nx_http::AsyncMessagePipeline>(
            nullptr,
            std::move(tcpSocket));
        httpMsgPipeline->startReadingConnection();

        nx::utils::promise<nx_http::Message> responseReceivedPromise;
        httpMsgPipeline->setMessageHandler(
            [&responseReceivedPromise](nx_http::Message msg)
            {
                responseReceivedPromise.set_value(std::move(msg));
            });
        httpMsgPipeline->sendMessage(std::move(requestMsg), [](SystemError::ErrorCode){});

        auto responseReceivedFuture = responseReceivedPromise.get_future();
        ASSERT_EQ(
            std::future_status::ready,
            responseReceivedFuture.wait_for(std::chrono::seconds(10)));
        const auto responseMsg = std::move(responseReceivedFuture.get());
        ASSERT_EQ(nx_http::MessageType::response, responseMsg.type);
        ASSERT_EQ(nx_http::StatusCode::ok, responseMsg.response->statusLine.statusCode);

        httpMsgPipeline->pleaseStopSync();
    }
};

#ifdef ENABLE_CLOUD_TEST
TEST_F(CloudAuthentication, all)
#else
TEST_F(CloudAuthentication, DISABLED_all)
#endif
{
    doGeneralTest();
    doNonceFromCloudDbTest();
}
