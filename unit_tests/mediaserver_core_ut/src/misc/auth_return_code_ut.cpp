#include <QFile>
#include <vector>

#include <gtest/gtest.h>
#include <server/server_globals.h>
#include "utils.h"
#include "mediaserver_launcher.h"
#include <api/app_server_connection.h>
#include <nx/network/http/httpclient.h>
#include <http/custom_headers.h>

#include <nx/fusion/model_functions.h>
#include <network/authenticate_helper.h>
#include <api/model/cookie_login_data.h>
#include <network/authutil.h>
#include <rest/server/json_rest_result.h>

class AuthReturnCodeTest : public ::testing::Test
{
public:
    static void SetUpTestCase()
    {
        mediaServerLauncher.reset(new MediaServerLauncher());
        ASSERT_TRUE(mediaServerLauncher->start());
    }

    static void TearDownTestCase()
    {
        mediaServerLauncher.reset();
    }

    virtual void SetUp() override
    {
        userData.id = QnUuid::createUuid();
        userData.name = "Vasja pupkin@gmail.com";
        userData.email = userData.name;
        userData.isEnabled = true;
        userData.isCloud = true;
        {
            auto ec2Connection = QnAppServerConnectionFactory::getConnection2();
            ec2::AbstractUserManagerPtr userManager = ec2Connection->getUserManager(Qn::kSystemAccess);
            ASSERT_EQ(ec2::ErrorCode::ok, userManager->saveSync(userData));
        }
    }

    ec2::ApiUserData userData;
    static std::unique_ptr<MediaServerLauncher> mediaServerLauncher;
};
std::unique_ptr<MediaServerLauncher> AuthReturnCodeTest::mediaServerLauncher;

void testServerReturnCode(
    const ec2::ApiUserData& userData,
    const MediaServerLauncher* mediaServerLauncher,
    nx_http::AsyncHttpClient::AuthType authType,
    int expectedStatusCode,
    Qn::AuthResult expectedAuthResult)
{
    // Waiting for server to come up

    nx_http::HttpClient httpClient;
    httpClient.setUserName(userData.name);
    httpClient.setUserPassword("invalid password");
    httpClient.setAuthType(authType);
    const auto startTime = std::chrono::steady_clock::now();
    constexpr const auto maxPeriodToWaitForMediaServerStart = std::chrono::seconds(150);
    while (std::chrono::steady_clock::now() - startTime < maxPeriodToWaitForMediaServerStart)
    {
        QUrl url = mediaServerLauncher->apiUrl();
        url.setPath("/ec2/getUsers");
        if (httpClient.doGet(url))
            break;  //< Server is alive
    }
    ASSERT_TRUE(httpClient.response());
    auto statusCode = httpClient.response()->statusLine.statusCode;
    ASSERT_EQ(expectedStatusCode, statusCode);

    QString authResultStr = nx_http::getHeaderValue(httpClient.response()->headers, Qn::AUTH_RESULT_HEADER_NAME);
    ASSERT_FALSE(authResultStr.isEmpty());

    Qn::AuthResult authResult = QnLexical::deserialized<Qn::AuthResult>(authResultStr);
    ASSERT_EQ(expectedAuthResult, authResult);
}

void testCookieAuth(
    const QString& login,
    const QString& password,
    const MediaServerLauncher* mediaServerLauncher,
    QnRestResult::Error expectedError)
{
    QnCookieData cookieLogin;
    cookieLogin.auth = createHttpQueryAuthParam(
        login,
        password,
        QnAppInfo::realm().toUtf8(),
        nx_http::Method::GET,
        QnAuthHelper::instance()->generateNonce());

    auto msgBody = QJson::serialized(cookieLogin);
    QUrl url = mediaServerLauncher->apiUrl();
    url.setPath("/api/cookieLogin");
    nx_http::HttpClient httpClient;
    httpClient.doPost(url, "application/json", msgBody);
    ASSERT_TRUE(httpClient.response());
    auto statusCode = httpClient.response()->statusLine.statusCode;

    QByteArray response;
    while (!httpClient.eof())
        response += httpClient.fetchMessageBodyBuffer();
    QnJsonRestResult jsonResult = QJson::deserialized<QnJsonRestResult>(response);
    ASSERT_EQ(expectedError, jsonResult.error);
}

TEST_F(AuthReturnCodeTest, authWhileRestart)
{
    // Cloud users is forbidden for basic auth.
    // We had bug: server return invalid code if request occurred when server is just started.
    mediaServerLauncher->stop();
    mediaServerLauncher->startAsync();
    testServerReturnCode(
        userData,
        mediaServerLauncher.get(),
        nx_http::AsyncHttpClient::authBasic,
        nx_http::StatusCode::forbidden,
        Qn::Auth_Forbidden);
}

TEST_F(AuthReturnCodeTest, noCloudConnect)
{
    // We have cloud user but not connected to cloud yet.
    testServerReturnCode(
        userData,
        mediaServerLauncher.get(),
        nx_http::AsyncHttpClient::authDigest,
        nx_http::StatusCode::unauthorized,
        Qn::Auth_CloudConnectError);

}

TEST_F(AuthReturnCodeTest, cookieNoCloudConnect)
{
    // We have cloud user but not connected to cloud yet.
    testCookieAuth(
        userData.name, "invalid password",
        mediaServerLauncher.get(),
        QnRestResult::CantProcessRequest);
}

TEST_F(AuthReturnCodeTest, cookieWrongPassword)
{
    // Check return code for wrong password
    testCookieAuth(
        "admin", "invalid password",
        mediaServerLauncher.get(),
        QnRestResult::InvalidParameter);
}

TEST_F(AuthReturnCodeTest, cookieCorrectPassword)
{
    // Check success auth
    testCookieAuth(
        "admin", "admin",
        mediaServerLauncher.get(),
        QnRestResult::NoError);
}
