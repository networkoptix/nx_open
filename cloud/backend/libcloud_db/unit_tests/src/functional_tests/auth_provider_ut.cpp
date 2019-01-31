#include <gtest/gtest.h>

#include <nx/network/app_info.h>

#include "test_setup.h"

namespace nx::cloud::db::test {

class AuthProvider:
    public CdbFunctionalTest
{
protected:
    virtual void SetUp() override
    {
        ASSERT_TRUE(startAndWaitUntilStarted());

        ASSERT_EQ(
            api::ResultCode::ok,
            addActivatedAccount(&m_account1, &m_account1Password));

        ASSERT_EQ(
            api::ResultCode::ok,
            addActivatedAccount(&m_account2, &m_account2Password));

        ASSERT_EQ(
            api::ResultCode::ok,
            bindRandomSystem(m_account1.email, m_account1Password, &m_system1));

        ASSERT_EQ(
            api::ResultCode::ok,
            shareSystem(m_account1.email, m_account1Password, m_system1.id,
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

private:
    api::AccountData m_account1;
    std::string m_account1Password;
    api::AccountData m_account2;
    std::string m_account2Password;
    api::SystemData m_system1;
    api::NonceData m_nonceData;
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

} // namespace nx::cloud::db::test
