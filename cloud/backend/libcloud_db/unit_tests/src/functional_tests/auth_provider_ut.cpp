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
            Credentials(QString::fromStdString(login), PasswordAuthToken(nx::String::fromStdString(password))),
            "/some/uri",
            wwwAuthenticate,
            &digestAuthorization);

        authorization.requestMethod = "GET";
        authorization.requestAuthorization = digestAuthorization.serialized().toStdString();

        m_connection->authProvider()->resolveUserCredentials(
            authorization,
            [this](auto&&... args)
            {
                m_resolveUserCredentialsResults.push(std::forward<decltype(args)>(args)...);
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

} // namespace nx::cloud::db::test
