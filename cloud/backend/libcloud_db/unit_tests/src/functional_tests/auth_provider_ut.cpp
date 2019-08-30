#include <gtest/gtest.h>

#include <nx/network/app_info.h>
#include <nx/network/http/auth_tools.h>
#include <nx/utils/thread/sync_queue.h>

#include "test_setup.h"

namespace nx::cloud::db::test {

class AuthProvider:
    public CdbFunctionalTest
{
protected:
    virtual void SetUp() override
    {
        ASSERT_TRUE(startAndWaitUntilStarted());

        m_connection = connection("", "");

        ASSERT_EQ(
            api::ResultCode::ok,
            addActivatedAccount(&m_account1, &m_account1.password));

        ASSERT_EQ(
            api::ResultCode::ok,
            addActivatedAccount(&m_account2, &m_account2.password));

        ASSERT_EQ(
            api::ResultCode::ok,
            bindRandomSystem(m_account1.email, m_account1.password, &m_system1));

        ASSERT_EQ(
            api::ResultCode::ok,
            shareSystem(m_account1.email, m_account1.password, m_system1.id,
                m_account2.email, api::SystemAccessRole::liveViewer));
    }

    void whenRequestNonce()
    {
        ASSERT_EQ(
            api::ResultCode::ok,
            getCdbNonce(m_system1.id, m_system1.authKey, m_system1.id, &m_nonceData));
    }

    void whenRequestAuthHash()
    {
        api::AuthRequest authRequest;
        authRequest.nonce = m_nonceData.nonce;
        authRequest.realm = nx::network::AppInfo::realm().toStdString();
        authRequest.username = m_account2.email;
        api::AuthResponse authResponse;
        ASSERT_EQ(
            api::ResultCode::ok,
            getAuthenticationResponse(m_system1.id, m_system1.authKey, authRequest, &authResponse));
    }

    void whenResolveCredentials(const std::string& login, const std::string& password)
    {
        m_connection->authProvider()->resolveUserCredentials(
            prepareUserAuthorization(login, password),
            [this](auto&&... args)
            {
                m_resolveUserCredentialsResults.push(std::forward<decltype(args)>(args)...);
            });
    }

    void whenGetSystemAccessLevel(
        const std::string& systemId,
        const std::string& login, const std::string& password)
    {
        m_connection->authProvider()->getSystemAccessLevel(
            systemId,
            prepareUserAuthorization(login, password),
            [this](api::ResultCode resultCode, api::SystemAccess systemAccess)
            {
                m_resolveSystemAccess.push({resultCode, {systemAccess}});
            });
    }

    struct Request
    {
        std::string systemId;
        std::string login;
        std::string password;
    };

    void whenGetSystemAccessLevel(std::vector<Request> testRequests)
    {
        std::vector<api::SystemAccessLevelRequest> requests;
        std::transform(
            testRequests.begin(), testRequests.end(),
            std::back_inserter(requests),
            [this](const Request& request)
            {
                return api::SystemAccessLevelRequest{
                    request.systemId,
                    prepareUserAuthorization(request.login, request.password)};
            });

        m_connection->authProvider()->getSystemAccessLevel(
            requests,
            [this](api::ResultCode resultCode, std::vector<api::SystemAccess> responses)
            {
                m_resolveSystemAccess.push(std::make_tuple(resultCode, std::move(responses)));
            });
    }

    void thenCredentialsAreResolved()
    {
        api::ResultCode resultCode = api::ResultCode::ok;
        std::tie(resultCode, m_lastResolvedCredentials) = m_resolveUserCredentialsResults.pop();
        ASSERT_EQ(api::ResultCode::ok, resultCode);
        ASSERT_EQ(api::ResultCode::ok, m_lastResolvedCredentials.status);
    }

    void andObjectTypeIs(api::ObjectType objectType)
    {
        ASSERT_EQ(objectType, m_lastResolvedCredentials.objectType);
    }

    void andObjectIdIs(const std::string& email)
    {
        ASSERT_EQ(email, m_lastResolvedCredentials.objectId);
    }

    void thenCredentialsAreNotResolved()
    {
        api::ResultCode resultCode = api::ResultCode::ok;
        std::tie(resultCode, m_lastResolvedCredentials) = m_resolveUserCredentialsResults.pop();
        ASSERT_EQ(api::ResultCode::ok, resultCode);
        ASSERT_NE(api::ResultCode::ok, m_lastResolvedCredentials.status);
    }

    void thenSystemAccessLevelIsProvided()
    {
        api::ResultCode resultCode = api::ResultCode::ok;
        std::tie(resultCode, m_lastResolvedSystemAccess) = m_resolveSystemAccess.pop();
        ASSERT_EQ(api::ResultCode::ok, resultCode);
    }

    void andAccessLevelIs(api::SystemAccessRole expected)
    {
        ASSERT_EQ(expected, m_lastResolvedSystemAccess.front().accessRole);
    }

    void andAccessLevelIs(std::vector<api::SystemAccessRole> expected)
    {
        ASSERT_EQ(expected.size(), m_lastResolvedSystemAccess.size());
        for (std::size_t i = 0; i < expected.size(); ++i)
            ASSERT_EQ(expected[i], m_lastResolvedSystemAccess[i].accessRole);
    }

    AccountWithPassword account() const
    {
        return m_account1;
    }

    api::SystemData system() const
    {
        return m_system1;
    }

private:
    std::unique_ptr<nx::cloud::db::api::Connection> m_connection;

    AccountWithPassword m_account1;
    AccountWithPassword m_account2;
    api::SystemData m_system1;
    api::NonceData m_nonceData;

    nx::utils::SyncQueue<std::tuple<api::ResultCode, api::CredentialsDescriptor>>
        m_resolveUserCredentialsResults;
    api::CredentialsDescriptor m_lastResolvedCredentials;

    nx::utils::SyncQueue<std::tuple<api::ResultCode, std::vector<api::SystemAccess>>>
        m_resolveSystemAccess;
    std::vector<api::SystemAccess> m_lastResolvedSystemAccess;

    api::UserAuthorization prepareUserAuthorization(
        const std::string& login,
        const std::string& password)
    {
        using namespace nx::network::http;

        api::UserAuthorization authorization;

        header::WWWAuthenticate wwwAuthenticate;
        wwwAuthenticate.authScheme = header::AuthScheme::digest;
        wwwAuthenticate.params["realm"] = nx::network::AppInfo::realm().toUtf8();
        wwwAuthenticate.params["algorithm"] = "MD5";
        wwwAuthenticate.params["nonce"] = "nonce";

        header::DigestAuthorization digestAuthorization;
        calcDigestResponse(
            "GET",
            Credentials(
                QString::fromStdString(login),
                PasswordAuthToken(nx::String::fromStdString(password))),
            "/some/uri",
            wwwAuthenticate,
            &digestAuthorization);

        authorization.requestMethod = "GET";
        authorization.requestAuthorization = digestAuthorization.serialized().toStdString();

        return authorization;
    }
};

TEST_F(AuthProvider, nonce)
{
    whenRequestNonce();
}

TEST_F(AuthProvider, auth)
{
    whenRequestNonce();
    whenRequestAuthHash();

    // thenAuthHashCanBeUsedToAuthenticateUser();
}

TEST_F(AuthProvider, resolve_account_credentials)
{
    whenResolveCredentials(account().email, account().password);

    thenCredentialsAreResolved();
    andObjectTypeIs(api::ObjectType::account);
    andObjectIdIs(account().email);
}

TEST_F(AuthProvider, resolve_system_credentials)
{
    whenResolveCredentials(system().id, system().authKey);

    thenCredentialsAreResolved();
    andObjectTypeIs(api::ObjectType::system);
    andObjectIdIs(system().id);
}

TEST_F(AuthProvider, resolve_bad_credentials_produces_error)
{
    whenResolveCredentials("Steven", "Spielberg");
    thenCredentialsAreNotResolved();
}

TEST_F(AuthProvider, reports_correct_system_permissions_to_owner)
{
    whenGetSystemAccessLevel(
        system().id,
        account().email, account().password);

    thenSystemAccessLevelIsProvided();
    andAccessLevelIs(api::SystemAccessRole::owner);
}

TEST_F(AuthProvider, reports_correct_system_permissions_to_mediaserver)
{
    whenGetSystemAccessLevel(
        system().id,
        system().id, system().authKey);

    thenSystemAccessLevelIsProvided();
    andAccessLevelIs(api::SystemAccessRole::system);
}

TEST_F(AuthProvider, reports_correct_system_permissions_batch)
{
    whenGetSystemAccessLevel(
        {{system().id, account().email, account().password},
            {system().id, system().id, system().authKey}});

    thenSystemAccessLevelIsProvided();

    andAccessLevelIs({
        api::SystemAccessRole::owner,
        api::SystemAccessRole::system});
}

} // namespace nx::cloud::db::test
