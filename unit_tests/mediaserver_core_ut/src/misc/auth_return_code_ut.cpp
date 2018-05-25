#include <QFile>
#include <vector>

#include <gtest/gtest.h>
#include <server/server_globals.h>
#include <test_support/utils.h>
#include <test_support/mediaserver_launcher.h>
#include <api/app_server_connection.h>
#include <nx/network/http/auth_tools.h>
#include <nx/network/http/http_client.h>
#include <nx/network/http/custom_headers.h>

#include <nx/fusion/model_functions.h>
#include <network/authenticate_helper.h>
#include <api/model/cookie_login_data.h>
#include <network/authutil.h>
#include <rest/server/json_rest_result.h>
#include "utils/common/sleep.h"
#include "common/common_module.h"
#include <api/global_settings.h>
#include <database/server_db.h>
#include <recording/time_period.h>
#include <audit/mserver_audit_manager.h>
#include <nx/utils/elapsed_timer.h>

class AuthReturnCodeTest:
    public ::testing::Test
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
        auto ec2Connection = mediaServerLauncher->commonModule()->ec2Connection();
        ec2::AbstractUserManagerPtr userManager = ec2Connection->getUserManager(Qn::kSystemAccess);

        userData.id = QnUuid::createUuid();
        userData.name = "Vasja pupkin@gmail.com";
        userData.email = userData.name;
        userData.isEnabled = true;
        userData.isCloud = true;
        ASSERT_EQ(ec2::ErrorCode::ok, userManager->saveSync(userData));

        ldapUserWithEmptyDigest.id = QnUuid::createUuid();
        ldapUserWithEmptyDigest.name = "ldap user 1";
        ldapUserWithEmptyDigest.isEnabled = true;
        ldapUserWithEmptyDigest.isLdap = true;
        ASSERT_EQ(ec2::ErrorCode::ok, userManager->saveSync(ldapUserWithEmptyDigest));

        ldapUserWithFilledDigest.id = QnUuid::createUuid();
        ldapUserWithFilledDigest.name = "ldap user 2";
        ldapUserWithFilledDigest.isEnabled = true;
        ldapUserWithFilledDigest.isLdap = true;

        auto hashes = PasswordData::calculateHashes(ldapUserWithFilledDigest.name, "some password", true);
        ldapUserWithFilledDigest.hash = hashes.passwordHash;
        ldapUserWithFilledDigest.digest = hashes.passwordDigest;

        ASSERT_EQ(ec2::ErrorCode::ok, userManager->saveSync(ldapUserWithFilledDigest));

        auto settings = mediaServerLauncher->commonModule()->globalSettings();
        settings->setCloudSystemId(QnUuid::createUuid().toString());
        settings->setCloudAuthKey(QnUuid::createUuid().toString());
        settings->synchronizeNowSync();
    }

    nx::utils::Url serverUrl(const QString& path) const
    {
        nx::utils::Url url = mediaServerLauncher->apiUrl();
        url.setPath(path);
        return url;
    }

    void addLocalUser(QString userName, QString password, bool isEnabled = true)
    {
        auto ec2Connection = mediaServerLauncher->commonModule()->ec2Connection();
        ec2::AbstractUserManagerPtr userManager = ec2Connection->getUserManager(Qn::kSystemAccess);

        userData.id = QnUuid::createUuid();
        userData.name = userName;
        userData.digest = nx::network::http::calcHa1(userName, nx::network::AppInfo::realm(), password);
        userData.isEnabled = isEnabled;
        userData.isCloud = false;
        ASSERT_EQ(ec2::ErrorCode::ok, userManager->saveSync(userData));
    }

    void assertServerAcceptsUserCredentials(QString userName, QString password)
    {
        testServerReturnCode(
            userName,
            password,
            nx::network::http::AuthType::authDigest,
            nx::network::http::StatusCode::ok,
            boost::none);
    }

    void testServerReturnCodeForWrongPassword(
        const ec2::ApiUserData& userDataToUse,
        nx::network::http::AuthType authType,
        int expectedStatusCode,
        Qn::AuthResult expectedAuthResult)
    {
        testServerReturnCode(
            userDataToUse.name,
            "invalid password",
            authType,
            expectedStatusCode,
            expectedAuthResult);
    }

    void testCookieAuth(
        const QString& login,
        const QString& password,
        QnRestResult::Error expectedError)
    {
        QnCookieData cookieLogin;
        cookieLogin.auth = createHttpQueryAuthParam(
            login,
            password,
            nx::network::AppInfo::realm().toUtf8(),
            nx::network::http::Method::get,
            QnAuthHelper::instance()->generateNonce());

        nx::network::http::HttpClient client;
        client.doPost(serverUrl("/api/cookieLogin"), "application/json", QJson::serialized(cookieLogin));
        ASSERT_TRUE(client.response());

        const auto result = QJson::deserialized<QnJsonRestResult>(*client.fetchEntireMessageBody());
        ASSERT_EQ(expectedError, result.error);
        if (result.error != QnRestResult::Error::NoError)
            return;

        // Set cookies manually, client does not support it.
        auto cookieMap = client.response()->getCookies();
        QStringList cookieList;
        for (const auto& it: cookieMap)
            cookieList << lm("%1=%2").args(it.first, it.second);

        const auto cookie = cookieList.join("; ").toUtf8();
        const auto csrf = cookieMap["nx-vms-csrf-token"];
        const auto doGet =
            [&](QString path, std::map<nx::String, nx::String> headers)
            {
                nx::network::http::HttpClient client;
                for (const auto& it: headers)
                    client.addAdditionalHeader(it.first, it.second);

                if (!client.doGet(serverUrl(path)) || !client.response())
                    return 0;

                return client.response()->statusLine.statusCode;
            };

        // Cookie works only with CSRF.
        ASSERT_EQ(401, doGet("/ec2/getUsers", {{"Cookie", cookie}}));
        ASSERT_EQ(200, doGet("/ec2/getUsers", {{"Cookie", cookie}, {"Nx-Vms-Csrf-Token", csrf}}));

        // Cookies does not work after logout.
        ASSERT_EQ(200, doGet("api/cookieLogout", {{"Cookie", cookie}, {"Nx-Vms-Csrf-Token", csrf}}));
        ASSERT_EQ(401, doGet("/ec2/getUsers", {{"Cookie", cookie}, {"Nx-Vms-Csrf-Token", csrf}}));
    }

    ec2::ApiUserData userData;
    ec2::ApiUserData ldapUserWithEmptyDigest;
    ec2::ApiUserData ldapUserWithFilledDigest;

    static std::unique_ptr<MediaServerLauncher> mediaServerLauncher;

    void testServerReturnCode(
        const QString& login,
        const QString& password,
        nx::network::http::AuthType authType,
        int expectedStatusCode,
        boost::optional<Qn::AuthResult> expectedAuthResult)
    {
        auto httpClient = std::make_unique<nx::network::http::HttpClient>();
        httpClient->setUserName(login);
        httpClient->setUserPassword(password);
        httpClient->setAuthType(authType);
        httpClient->addAdditionalHeader(Qn::CUSTOM_CHANGE_REALM_HEADER_NAME, QByteArray());
        testServerReturnCode(std::move(httpClient), expectedStatusCode, expectedAuthResult);
    }

    void testServerReturnCode(
        std::unique_ptr<nx::network::http::HttpClient> httpClient,
        int expectedStatusCode,
        boost::optional<Qn::AuthResult> expectedAuthResult = boost::none)
    {
        const auto startTime = std::chrono::steady_clock::now();
        constexpr const auto maxPeriodToWaitForMediaServerStart = std::chrono::seconds(150);
        while (std::chrono::steady_clock::now() - startTime < maxPeriodToWaitForMediaServerStart)
        {
            if (mediaServerLauncher->port() == 0)
            {
                std::this_thread::sleep_for(std::chrono::milliseconds(1));
                continue;
            }
            if (httpClient->doGet(serverUrl("/ec2/getUsers")))
                break;  //< Server is alive
        }
        ASSERT_TRUE(httpClient->response());
        auto statusCode = httpClient->response()->statusLine.statusCode;
        ASSERT_EQ(expectedStatusCode, statusCode);

        if (expectedAuthResult)
        {
            QString authResultStr = nx::network::http::getHeaderValue(
                httpClient->response()->headers, Qn::AUTH_RESULT_HEADER_NAME);
            ASSERT_FALSE(authResultStr.isEmpty());

            Qn::AuthResult authResult = QnLexical::deserialized<Qn::AuthResult>(authResultStr);
            ASSERT_EQ(expectedAuthResult, authResult);

            if (expectedStatusCode == nx::network::http::StatusCode::unauthorized)
            {
                httpClient.reset(); //< Ensure connection is closed.

                QnTimePeriod period(0, QnTimePeriod::infiniteDuration());
                QnAuditRecordList outputData;
                static const std::chrono::seconds kMaxWaitTime(10);
                nx::utils::ElapsedTimer timer;
                timer.restart();
                // Server send "Unauthorized" response before it write data to the auditLog.
                do
                {
                    static_cast<QnMServerAuditManager*>(qnAuditManager)->flushRecords();
                    outputData = qnServerDb->getAuditData(period, QnUuid());
                    if (outputData.isEmpty())
                        std::this_thread::sleep_for(std::chrono::milliseconds(1));
                } while (outputData.isEmpty() && timer.elapsed() < kMaxWaitTime);
                ASSERT_TRUE(!outputData.isEmpty());
                ASSERT_EQ(Qn::AuditRecordType::AR_UnauthorizedLogin, outputData.last().eventType);
            }
        }
    }
};

std::unique_ptr<MediaServerLauncher> AuthReturnCodeTest::mediaServerLauncher;

TEST_F(AuthReturnCodeTest, authWhileRestart)
{
    // Cloud users is forbidden for basic auth.
    // We had bug: server return invalid code if request occurred when server is just started.
    mediaServerLauncher->stop();
    mediaServerLauncher->startAsync();
    testServerReturnCodeForWrongPassword(
        userData,
        nx::network::http::AuthType::authBasic,
        nx::network::http::StatusCode::forbidden,
        Qn::Auth_Forbidden);
}

TEST_F(AuthReturnCodeTest, noCloudConnect)
{
    // We have cloud user but not connected to cloud yet.
    testServerReturnCodeForWrongPassword(
        userData,
        nx::network::http::AuthType::authDigest,
        nx::network::http::StatusCode::unauthorized,
        Qn::Auth_CloudConnectError);

}

TEST_F(AuthReturnCodeTest, cookieNoCloudConnect)
{
    // We have cloud user but not connected to cloud yet.
    testCookieAuth(
        userData.name, "invalid password",
        QnRestResult::CantProcessRequest);
}

TEST_F(AuthReturnCodeTest, cookieWrongPassword)
{
    // Check return code for wrong password
    testCookieAuth(
        "admin", "invalid password",
        QnRestResult::InvalidParameter);
}

TEST_F(AuthReturnCodeTest, cookieCorrectPassword)
{
    // Check success auth
    testCookieAuth(
        "admin", "admin",
        QnRestResult::NoError);
}

TEST_F(AuthReturnCodeTest, localUserWithCloudLikeName)
{
    const QString userCredentials[] = {"username@", "password"};

    addLocalUser(userCredentials[0], userCredentials[1]);
    assertServerAcceptsUserCredentials(userCredentials[0], userCredentials[1]);
}

TEST_F(AuthReturnCodeTest, disabledUser)
{
    const QString userCredentials[] = { "test2", "password" };

    addLocalUser(userCredentials[0], userCredentials[1], false);
    testServerReturnCode(
        userCredentials[0],
        userCredentials[1],
        nx::network::http::AuthType::authDigest,
        nx::network::http::StatusCode::unauthorized,
        Qn::Auth_DisabledUser);
}

TEST_F(AuthReturnCodeTest, noLdapConnect)
{
    // We have cloud user but not connected to cloud yet.
    testServerReturnCodeForWrongPassword(
        ldapUserWithEmptyDigest,
        nx::network::http::AuthType::authBasicAndDigest,
        nx::network::http::StatusCode::unauthorized,
        Qn::Auth_LDAPConnectError);

    // We have cloud user but not connected to cloud yet.
    testServerReturnCodeForWrongPassword(
        ldapUserWithFilledDigest,
        nx::network::http::AuthType::authDigest,
        nx::network::http::StatusCode::unauthorized,
        Qn::Auth_LDAPConnectError);
}

static std::unique_ptr<nx::network::http::HttpClient> manualDigestClient(
    const QByteArray& method, const QByteArray& uri)
{
    nx::network::http::header::WWWAuthenticate unquthorizedHeader;
    EXPECT_TRUE(unquthorizedHeader.parse(
        "Digest realm=\"VMS\", "
        "nonce=\"" + QnAuthHelper::instance()->generateNonce() + "\", "
        "algorithm=MD5"));

    nx::network::http::header::DigestAuthorization digestHeader;
    EXPECT_TRUE(nx::network::http::calcDigestResponse(
        method, "admin", QByteArray("admin"), boost::none, uri,
        unquthorizedHeader, &digestHeader));

    auto client = std::make_unique<nx::network::http::HttpClient>();
    client->setDisablePrecalculatedAuthorization(true);
    client->addAdditionalHeader(nx::network::http::header::Authorization::NAME, digestHeader.serialized());
    return client;
}

TEST_F(AuthReturnCodeTest, manualDigest)
{
    auto client = manualDigestClient("GET", "/ec2/getUsers");
    testServerReturnCode(std::move(client), 200);
}

TEST_F(AuthReturnCodeTest, manualDigestWrongUri)
{
    auto client = manualDigestClient("GET", "/api/getCameras");
    testServerReturnCode(std::move(client), 401, Qn::AuthResult::Auth_WrongDigest);
}

TEST_F(AuthReturnCodeTest, manualDigestWrongMethod)
{
    auto client = manualDigestClient("POST", "/api/getUsers");
    testServerReturnCode(std::move(client), 401, Qn::AuthResult::Auth_WrongDigest);
}

TEST_F(AuthReturnCodeTest, manualDigest_onGateway)
{
    auto client = manualDigestClient("GET", "/gateway/c6901e5b-4884-41f4-ab80-73e078341252/ec2/getUsers");
    testServerReturnCode(std::move(client), 200);
}

TEST_F(AuthReturnCodeTest, manualDigestWrongUri_onGateway)
{
    auto client = manualDigestClient("GET", "/gateway/c6901e5b-4884-41f4-ab80-73e078341252/api/getCameras");
    testServerReturnCode(std::move(client), 401, Qn::AuthResult::Auth_WrongDigest);
}

TEST_F(AuthReturnCodeTest, manualDigestWrongMethod_onGateway)
{
    auto client = manualDigestClient("POST", "/api/getUsers");
    testServerReturnCode(std::move(client), 401, Qn::AuthResult::Auth_WrongDigest);
}
