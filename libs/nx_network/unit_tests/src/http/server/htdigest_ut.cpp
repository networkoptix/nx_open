#include <gtest/gtest.h>

#include <nx/network/url/url_builder.h>
#include <nx/network/http/http_client.h>
#include <nx/network/http/test_http_server.h>
#include <nx/network/http/server/http_server_htdigest_authentication_provider.h>

namespace nx::network::http::server::test {

namespace
{

static const std::vector<std::pair<const char*, const char*>> kUserAndPassword =
{
    {"user1", "7ab592129c1345326c0cea345e71170a"},
    {"user2", "3064acea7f7be21564a2a9cac41e1807"},
    {"user3", "4e4860fd5eca841bde1f477f5ae3345b"},
    {"user4", "231c6918f64d96d861009a3ee5e22421"},
};

static const std::pair<const char*, const char*> kUnknownUserAndPassword("unknown_user", "");

} // namespace

class HtDigestAuthenticationProvider:
    public ::testing::Test
{
protected:
    std::stringstream getValidInputStream()
    {
        std::stringstream input;
        for (std::size_t i = 0; i < kUserAndPassword.size(); ++i)
        {
            input << kUserAndPassword[i].first
                << std::string(":realm") + std::to_string(i) + ":"
                << kUserAndPassword[i].second
                << '\n';
        }
        return input;
    }

    std::stringstream getMalformedInputStream()
    {
        std::stringstream input;

        // OK.
        input << kUserAndPassword[0].first << ":administrator:" << kUserAndPassword[0].second
            << '\n';

        // Missing first ":".
        input << kUserAndPassword[1].first << "owner:" << kUserAndPassword[1].second << '\n';

        // Missing second ":".
        input << kUserAndPassword[2].first << ":manager" << kUserAndPassword[2].second << '\n';

        // Missing username.
        input << ":basic_user:" << kUserAndPassword[3].second << '\n';

        return input;
    }

    void whenCreateAuthenticationProviderWithValidInputStream()
    {
        std::stringstream input = getValidInputStream();
        m_authenticationProvider = std::make_unique<server::HtDigestAuthenticationProvider>(input);
    }

    void whenCreateAuthenticationProviderWithMalformedInputStream()
    {
        std::stringstream input = getMalformedInputStream();
        m_authenticationProvider = std::make_unique<server::HtDigestAuthenticationProvider>(input);
    }

    void thenPasswordLookupSucceeds(const std::pair<const char*, const char*>& userAndPassword)
    {
        m_authenticationProvider->getPasswordByUserName(userAndPassword.first,
            [userAndPassword](PasswordLookupResult result)
            {
                ASSERT_EQ(PasswordLookupResult::Code::ok, result.code);
                ASSERT_EQ(PasswordLookupResult::DataType::ha1, result.type);
                ASSERT_EQ(userAndPassword.second, *result.ha1());
            });
    }

    void thenPasswordLookupFails(const std::pair<const char*, const char*>& user)
    {
        m_authenticationProvider->getPasswordByUserName(user.first,
            [](PasswordLookupResult result)
            {
                ASSERT_NE(result.code, PasswordLookupResult::Code::ok);
            });
    }

    void thenValidPasswordLookupsSucceed()
    {
        thenPasswordLookupSucceeds(kUserAndPassword[0]);
    }

    void thenAllPasswordLookupsSucceed()
    {
        for(const auto& element : kUserAndPassword)
            thenPasswordLookupSucceeds(element);
    }

    void thenPasswordLookupFailsForUnknownUser()
    {
        thenPasswordLookupFails(kUnknownUserAndPassword);
    }

    void thenAllMalformedUserPasswordCombinationLookupsFail()
    {
        // Skipping user 0 as they are OK.
        for (std::size_t i = 1; i < kUserAndPassword.size(); ++i)
            thenPasswordLookupFails(kUserAndPassword[i]);
    }

protected:
    std::unique_ptr <server::HtDigestAuthenticationProvider> m_authenticationProvider;
};

TEST_F(HtDigestAuthenticationProvider, provides_passwords_for_all_known_users)
{
    whenCreateAuthenticationProviderWithValidInputStream();

    thenAllPasswordLookupsSucceed();
}

TEST_F(HtDigestAuthenticationProvider, rejects_password_lookup_for_unknown_user)
{
    whenCreateAuthenticationProviderWithValidInputStream();

    thenPasswordLookupFailsForUnknownUser();
}

TEST_F(HtDigestAuthenticationProvider,
    provides_password_for_valid_user_after_loading_malformed_input_stream)
{
    whenCreateAuthenticationProviderWithMalformedInputStream();

    thenValidPasswordLookupsSucceed();
}

TEST_F(HtDigestAuthenticationProvider, rejects_malformed_input)
{
    whenCreateAuthenticationProviderWithMalformedInputStream();

    thenAllMalformedUserPasswordCombinationLookupsFail();
}

} // namespace nx::network::http::server::test