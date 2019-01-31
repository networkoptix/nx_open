#include <QFile>
#include <vector>

#include <gtest/gtest.h>

#include <api/app_server_connection.h>
#include <api/global_settings.h>
#include <api/model/cookie_login_data.h>
#include <audit/mserver_audit_manager.h>
#include <common/common_module.h>
#include <core/resource_management/resource_pool.h>
#include <core/resource/media_server_resource.h>
#include <database/server_db.h>
#include <network/authutil.h>
#include <nx/fusion/model_functions.h>
#include <nx/vms/server/authenticator.h>
#include <nx/network/app_info.h>
#include <nx/network/http/auth_tools.h>
#include <nx/network/http/custom_headers.h>
#include <nx/network/http/http_client.h>
#include <nx/utils/elapsed_timer.h>
#include <recording/time_period.h>
#include <rest/server/json_rest_result.h>
#include <server/server_globals.h>
#include <test_support/mediaserver_launcher.h>
#include <test_support/utils.h>
#include <utils/common/sleep.h>
#include <core/resource/user_resource.h>

// TODO: Major refactring is required:
// - Get rid of number http codes, use constants intead.
// - Replace hardcoded credentials with constants.

namespace nx {
namespace vms::server {
namespace test {

class LdapManagerMock: public AbstractLdapManager
{
public:
    virtual LdapResult fetchUsers(QnLdapUsers &users, const QnLdapSettings& settings) override
    {
        return LdapResult();
    }

    virtual LdapResult fetchUsers(QnLdapUsers &users) override
    {
        return LdapResult();
    }

    virtual Qn::AuthResult authenticate(const QString& login, const QString& password) override
    {
        bool success = m_ldapPassword == password;
        return success ? Qn::Auth_OK : Qn::Auth_WrongPassword;
    }

    void setPassword(const QString& value)
    {
        m_ldapPassword = value;
    }
private:
    QString m_ldapPassword;
};

class AuthenticationTest: public QObject, public ::testing::Test
{
public:
    static void SetUpTestCase()
    {
        server.reset(new MediaServerLauncher());
        ASSERT_TRUE(server->start());
    }

    static void TearDownTestCase()
    {
        server.reset();
    }

    virtual void SetUp() override
    {
        server->authenticator()->setLockoutOptions(std::nullopt);
        server->authenticator()->setLdapManager(std::make_unique<LdapManager>(server->commonModule()));

        auto ec2Connection = server->commonModule()->ec2Connection();
        ec2::AbstractUserManagerPtr userManager = ec2Connection->getUserManager(Qn::kSystemAccess);

        userData.name = "Vasja pupkin@gmail.com";
        userData.id = guidFromArbitraryData(userData.name);
        userData.email = userData.name;
        userData.isEnabled = true;
        userData.isCloud = true;
        ASSERT_EQ(ec2::ErrorCode::ok, userManager->saveSync(userData));

        ldapUserWithEmptyDigest.name = "ldap user 1";
        ldapUserWithEmptyDigest.id = guidFromArbitraryData(ldapUserWithEmptyDigest.name);
        ldapUserWithEmptyDigest.isEnabled = true;
        ldapUserWithEmptyDigest.isLdap = true;
        ASSERT_EQ(ec2::ErrorCode::ok, userManager->saveSync(ldapUserWithEmptyDigest));

        ldapUserWithFilledDigest.name = "ldap user 2";
        ldapUserWithFilledDigest.id = guidFromArbitraryData(ldapUserWithFilledDigest.name);
        ldapUserWithFilledDigest.isEnabled = true;
        ldapUserWithFilledDigest.isLdap = true;

        auto hashes = PasswordData::calculateHashes(ldapUserWithFilledDigest.name, "some password", true);
        ldapUserWithFilledDigest.hash = hashes.passwordHash;
        ldapUserWithFilledDigest.digest = hashes.passwordDigest;

        ASSERT_EQ(ec2::ErrorCode::ok, userManager->saveSync(ldapUserWithFilledDigest));

        auto settings = server->commonModule()->globalSettings();
        settings->setCloudSystemId(QnUuid::createUuid().toString());
        settings->setCloudAuthKey(QnUuid::createUuid().toString());
        settings->synchronizeNowSync();
    }

    nx::utils::Url serverUrl(const QString& path, const QString& query = {}) const
    {
        nx::utils::Url url = server->apiUrl();
        url.setPath(path);
        url.setQuery(query);
        return url;
    }

    void addLocalUser(QString userName, QString password, bool isEnabled = true)
    {
        auto ec2Connection = server->commonModule()->ec2Connection();
        ec2::AbstractUserManagerPtr userManager = ec2Connection->getUserManager(Qn::kSystemAccess);

        userData.id = QnUuid::createUuid();
        userData.name = userName;
        userData.realm = nx::network::AppInfo::realm();
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

    void assertServerRejectsUserCredentials(QString userName, QString password, Qn::AuthResult error)
    {
        testServerReturnCode(
            userName,
            password,
            nx::network::http::AuthType::authDigest,
            nx::network::http::StatusCode::unauthorized,
            error);
    }

    void testServerReturnCodeForWrongPassword(
        const nx::vms::api::UserData& userDataToUse,
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
        QnRestResult::Error expectedError,
        bool withCsrf = true)
    {
        QnCookieData cookieLogin;
        cookieLogin.auth = createHttpQueryAuthParam(
            login,
            password,
            nx::network::AppInfo::realm().toUtf8(),
            nx::network::http::Method::get,
            server->authenticator()->generateNonce());

        nx::network::http::HttpClient client;
        client.doPost(serverUrl("/api/cookieLogin"), "application/json", QJson::serialized(cookieLogin));
        ASSERT_TRUE(client.response());

        const auto result = QJson::deserialized<QnJsonRestResult>(*client.fetchEntireMessageBody());
        ASSERT_EQ(expectedError, result.error);
        if (result.error != QnRestResult::Error::NoError)
            return;

        // Set cookies manually, HTTP client does not support it.
        auto cookieMap = client.response()->getCookies();
        QStringList cookieList;
        for (const auto& it: cookieMap)
            cookieList << lm("%1=%2").args(it.first, it.second);

        const auto cookie = cookieList.join("; ").toUtf8();
        const auto session = cookieMap["x-runtime-guid"];
        if (withCsrf)
        {
            // Cookie works only with extra header.
            expectGetResult("/ec2/getUsers", {{"Cookie", cookie}});
            expectGetResult("/ec2/getUsers", {{"Cookie", cookie}, {"X-Runtime-Guid", session}});

            // Cookies does not work after logout.
            expectGetResult("/api/cookieLogout", {{"Cookie", cookie}, {"X-Runtime-Guid", session}});
            expectGetResult("/ec2/getUsers", {{"Cookie", cookie}, {"X-Runtime-Guid", session}}, 401);
        }
        else
        {
            expectGetResult("/api/showLog", {{"Cookie", cookie}}, 400);
            expectGetResult("/api/cookieLogout", {{"Cookie", cookie}});
            expectGetResult("/api/showLog", {{"Cookie", cookie}}, 401);
        }
    }

    void expectGetResult(
        const QString& path, std::map<nx::String, nx::String> headers, int expectedStatus = 200)
    {
        nx::network::http::HttpClient client;
        for (const auto& it: headers)
            client.addAdditionalHeader(it.first, it.second);

        ASSERT_TRUE(client.doGet(serverUrl(path))) << path.toStdString();
        ASSERT_TRUE(client.response()) << path.toStdString();
        ASSERT_EQ(expectedStatus, client.response()->statusLine.statusCode) << path.toStdString();
    }

    nx::vms::api::UserData userData;
    nx::vms::api::UserData ldapUserWithEmptyDigest;
    nx::vms::api::UserData ldapUserWithFilledDigest;

    static std::unique_ptr<MediaServerLauncher> server;

    void testServerReturnCode(
        const QString& login,
        const QString& password,
        nx::network::http::AuthType authType,
        int expectedStatusCode,
        boost::optional<Qn::AuthResult> expectedAuthResult = boost::none)
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
            if (server->port() == 0)
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
            ASSERT_EQ(*expectedAuthResult, authResult) << authResultStr.toStdString();

            if (expectedStatusCode == nx::network::http::StatusCode::unauthorized)
            {
                httpClient.reset(); //< Ensure connection is closed.

                const auto period = QnTimePeriod::anytime();
                QnAuditRecordList outputData;
                static const std::chrono::seconds kMaxWaitTime(10);
                nx::utils::ElapsedTimer timer;
                timer.restart();
                // Server send "Unauthorized" response before it write data to the auditLog.
                do
                {
                    static_cast<QnMServerAuditManager*>(server->commonModule()->auditManager())->flushRecords();
                    outputData = server->serverModule()->serverDb()->getAuditData(period, QnUuid());
                    if (outputData.isEmpty())
                        std::this_thread::sleep_for(std::chrono::milliseconds(1));
                } while (outputData.isEmpty() && timer.elapsed() < kMaxWaitTime);
                ASSERT_TRUE(!outputData.isEmpty());
                ASSERT_EQ(Qn::AuditRecordType::AR_UnauthorizedLogin, outputData.last().eventType);
            }
        }
    }

    std::unique_ptr<nx::network::http::HttpClient> manualDigestClient(
        const QByteArray& method, const QByteArray& uri)
    {
        nx::network::http::header::WWWAuthenticate unquthorizedHeader;
        EXPECT_TRUE(unquthorizedHeader.parse(
            "Digest realm=\"VMS\", "
            "nonce=\"" + server->authenticator()->generateNonce() + "\", "
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

    QnUserResourcePtr getUser(const nx::vms::api::UserData& userData)
    {
        auto resourcePool = server->commonModule()->resourcePool();
        return resourcePool->getResources<QnUserResource>().filtered(
            [&userData](const QnUserResourcePtr& user)
            {
                return user->getName() == userData.name;
            }).first();
    }

    void waitCachedPasswordToExpire(const QnUserResourcePtr& ldapUser)
    {
        std::promise<bool> isSessionExpired;
        QObject::connect(ldapUser.data(), &QnUserResource::sessionExpired, this,
            [&isSessionExpired]()
            {
                isSessionExpired.set_value(true);
            }, Qt::DirectConnection);
        if (!ldapUser->passwordExpired())
            isSessionExpired.get_future().wait();
    }
};

std::unique_ptr<MediaServerLauncher> AuthenticationTest::server;

TEST_F(AuthenticationTest, authWhileRestart)
{
    // Cloud users is forbidden for basic auth.
    // We had bug: server return invalid code if request occurred when server is just started.
    server->stop();
    server->startAsync();
    testServerReturnCodeForWrongPassword(
        userData,
        nx::network::http::AuthType::authBasic,
        nx::network::http::StatusCode::forbidden,
        Qn::Auth_Forbidden);
}

TEST_F(AuthenticationTest, noCloudConnect)
{
    // We have cloud user but not connected to cloud yet.
    testServerReturnCodeForWrongPassword(
        userData,
        nx::network::http::AuthType::authDigest,
        nx::network::http::StatusCode::unauthorized,
        Qn::Auth_CloudConnectError);

}

TEST_F(AuthenticationTest, cookieNoCloudConnect)
{
    // We have cloud user but not connected to cloud yet.
    testCookieAuth(
        userData.name, "invalid password",
        QnRestResult::CantProcessRequest);
}

TEST_F(AuthenticationTest, cookieWrongPassword)
{
    // Check return code for wrong password
    testCookieAuth(
        "admin", "invalid password",
        QnRestResult::InvalidParameter);
}

TEST_F(AuthenticationTest, cookieCorrectPassword)
{
    testCookieAuth("admin", "admin", QnRestResult::NoError);
}

TEST_F(AuthenticationTest, cookieWithoutScrf)
{
    testCookieAuth("admin", "admin", QnRestResult::NoError, /*withCsrf*/ false);
}

TEST_F(AuthenticationTest, localUserWithCloudLikeName)
{
    const QString userCredentials[] = {"username@", "password"};

    addLocalUser(userCredentials[0], userCredentials[1]);
    assertServerAcceptsUserCredentials(userCredentials[0], userCredentials[1]);
}

TEST_F(AuthenticationTest, disabledUser)
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

TEST_F(AuthenticationTest, noLdapConnect)
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
        nx::network::http::AuthType::authBasicAndDigest, //< TODO: should be set to only digest.
        nx::network::http::StatusCode::unauthorized,
        Qn::Auth_LDAPConnectError);
}

TEST_F(AuthenticationTest, ldapCachedPasswordHasExpired)
{
    static const QString kLdapUserPassword("password1");
    static const QString kNewLdapUserPassword("password2");

    auto ldapManager = std::make_unique<LdapManagerMock>();
    auto ldapManagerPtr = ldapManager.get();
    server->authenticator()->setLdapManager(std::move(ldapManager));
    ldapManagerPtr->setPassword(kLdapUserPassword); //< Password for the LDAP Server.

    getUser(ldapUserWithEmptyDigest)->setLdapPasswordExperationPeriod(std::chrono::milliseconds(1));

    testServerReturnCode(
        ldapUserWithEmptyDigest.name,
        kLdapUserPassword,
        nx::network::http::AuthType::authBasicAndDigest,
        nx::network::http::StatusCode::ok); //< Password should match.

    waitCachedPasswordToExpire(getUser(ldapUserWithEmptyDigest));
    ldapManagerPtr->setPassword(kNewLdapUserPassword); //< LDAP server has changed password.

    testServerReturnCode(
        ldapUserWithEmptyDigest.name,
        kLdapUserPassword,
        nx::network::http::AuthType::authBasicAndDigest,
        nx::network::http::StatusCode::unauthorized); //< Password should not match.
}

TEST_F(AuthenticationTest, manualDigest)
{
    auto client = manualDigestClient("GET", "/ec2/getUsers");
    testServerReturnCode(std::move(client), 200);
}

TEST_F(AuthenticationTest, manualDigestWrongUri)
{
    auto client = manualDigestClient("GET", "/api/getCameras");
    testServerReturnCode(std::move(client), 401, Qn::AuthResult::Auth_WrongDigest);
}

TEST_F(AuthenticationTest, manualDigestWrongMethod)
{
    auto client = manualDigestClient("POST", "/api/getUsers");
    testServerReturnCode(std::move(client), 401, Qn::AuthResult::Auth_WrongDigest);
}

TEST_F(AuthenticationTest, manualDigest_onGateway)
{
    auto client = manualDigestClient("GET", "/gateway/c6901e5b-4884-41f4-ab80-73e078341252/ec2/getUsers");
    testServerReturnCode(std::move(client), 200);
}

TEST_F(AuthenticationTest, manualDigestWrongUri_onGateway)
{
    auto client = manualDigestClient("GET", "/gateway/c6901e5b-4884-41f4-ab80-73e078341252/api/getCameras");
    testServerReturnCode(std::move(client), 401, Qn::AuthResult::Auth_WrongDigest);
}

TEST_F(AuthenticationTest, manualDigestWrongMethod_onGateway)
{
    auto client = manualDigestClient("POST", "/api/getUsers");
    testServerReturnCode(std::move(client), 401, Qn::AuthResult::Auth_WrongDigest);
}

static const Authenticator::LockoutOptions kLockoutOptions{
    3, std::chrono::seconds(10), std::chrono::seconds(1)};

TEST_F(AuthenticationTest, lockoutTest)
{
    server->authenticator()->setLockoutOptions(kLockoutOptions);

    // Lockout does not happen on a random attemp.
    assertServerAcceptsUserCredentials("admin", "admin");
    assertServerRejectsUserCredentials("admin", "qweasd123", Qn::Auth_WrongPassword);
    assertServerAcceptsUserCredentials("admin", "admin");

    // Trigger lockout.
    for (int i = 0; i < kLockoutOptions.maxLoginFailures - 1; ++i)
        assertServerRejectsUserCredentials("admin", "qweasd123", Qn::Auth_WrongPassword);
    assertServerRejectsUserCredentials("admin", "admin", Qn::Auth_LockedOut);

    // Wait for lockout to clear.
    std::this_thread::sleep_for(kLockoutOptions.lockoutTime);
    assertServerAcceptsUserCredentials("admin", "admin");
}

TEST_F(AuthenticationTest, sessionKey)
{
    // Initial request to authorize session.
    const auto sessionKey = QnUuid::createUuid().toByteArray();
    {
        auto client = std::make_unique<nx::network::http::HttpClient>();
        client->setUserName("admin");
        client->setUserPassword("admin");
        client->addAdditionalHeader(Qn::EC2_RUNTIME_GUID_HEADER_NAME, sessionKey);
        testServerReturnCode(std::move(client), 200);
    }

    // When session is authorized, login/password is not needed.
    {
        auto client = std::make_unique<nx::network::http::HttpClient>();
        client->addAdditionalHeader(Qn::EC2_RUNTIME_GUID_HEADER_NAME, sessionKey);
        testServerReturnCode(std::move(client), 200);
    }

    // Cause user to lockout.
    server->authenticator()->setLockoutOptions(kLockoutOptions);
    for (int i = 0; i < kLockoutOptions.maxLoginFailures; ++i)
        assertServerRejectsUserCredentials("admin", "qweasd123", Qn::Auth_WrongPassword);
    assertServerRejectsUserCredentials("admin", "admin", Qn::Auth_LockedOut);

    // Authorized session still works.
    {
        auto client = std::make_unique<nx::network::http::HttpClient>();
        client->addAdditionalHeader(Qn::EC2_RUNTIME_GUID_HEADER_NAME, sessionKey);
        testServerReturnCode(std::move(client), 200);
    }
}

TEST_F(AuthenticationTest, authKeyInUrl)
{
    auto client = std::make_unique<nx::network::http::HttpClient>();

    ASSERT_TRUE(client->doGet(serverUrl("/ec2/getUsers")));
    ASSERT_TRUE(client->response());
    ASSERT_EQ(401, client->response()->statusLine.statusCode);

    const auto queryItem = server->authenticator()->makeQueryItemForPath(
        Qn::kSystemAccess, "/ec2/getUsers");

    QUrlQuery query;
    query.addQueryItem(queryItem.first, queryItem.second);

    ASSERT_TRUE(client->doGet(serverUrl("/ec2/getUsers", query.toString())));
    ASSERT_TRUE(client->response());
    ASSERT_EQ(200, client->response()->statusLine.statusCode);

    ASSERT_TRUE(client->doGet(serverUrl("/ec2/getUsersEx", query.toString())));
    ASSERT_TRUE(client->response());
    ASSERT_EQ(401, client->response()->statusLine.statusCode);
}

TEST_F(AuthenticationTest, serverKey)
{
    const auto commonModule = server->commonModule();
    const auto serverResource = commonModule->resourcePool()
        ->getResourceById<QnMediaServerResource>(commonModule->moduleGUID());

    const auto login = serverResource->getId().toString();
    const auto makeClient =
        [&](const QString& password)
        {
            auto client = std::make_unique<nx::network::http::HttpClient>();
            client->setUserName(login);
            client->setUserPassword(password);
            return client;
        };

    testServerReturnCode(
        makeClient(serverResource->getAuthKey()),
        nx::network::http::StatusCode::ok);

    testServerReturnCode(
        makeClient("invalid password"),
        nx::network::http::StatusCode::unauthorized, Qn::Auth_WrongPassword);

    auto client = makeClient(serverResource->getAuthKey());
    client->addAdditionalHeader(Qn::CUSTOM_USERNAME_HEADER_NAME, userData.name.toUtf8());
    testServerReturnCode(std::move(client), nx::network::http::StatusCode::ok);
}

} // namespace test
} // namespace vms::server
} // namespace nx
