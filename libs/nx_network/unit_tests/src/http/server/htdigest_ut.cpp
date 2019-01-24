#include <gtest/gtest.h>

#include <nx/network/url/url_builder.h>
#include <nx/network/http/http_client.h>
#include <nx/network/http/test_http_server.h>
#include <nx/network/http/server/http_server_htdigest_authentication_provider.h>

namespace nx::network::http::server::test {

namespace
{

static std::pair<nx::String, nx::String> kUser1("user1", "7ab592129c1345326c0cea345e71170a");
static std::pair<nx::String, nx::String> kUser2("user2", "3064acea7f7be21564a2a9cac41e1807");
static std::pair<nx::String, nx::String> kUser3("user3", "4e4860fd5eca841bde1f477f5ae3345b");
static std::pair<nx::String, nx::String> kUser4("user4", "231c6918f64d96d861009a3ee5e22421");
static std::pair<nx::String, nx::String> kUnknownUser("user5", "");

} // namespace

class HtDigest:
    public ::testing::Test
{
protected:
    void SetUp() override
    {
    }

    std::stringstream getDefaultInputStream()
    {
        std::stringstream input;
        input << kUser1.first.toStdString() << ":admin:" << kUser1.second.toStdString() << '\n';
        input << kUser2.first.toStdString() << ":owner:" << kUser2.second.toStdString() << '\n';
        input << kUser3.first.toStdString() << ":manager:" << kUser3.second.toStdString() << '\n';
        input << kUser4.first.toStdString() << ":basic_user:" << kUser4.second.toStdString()
            << '\n';
        return input;
    }

    std::stringstream getMalformedInputStream()
    {
        std::stringstream input;

        // OK.
        input << kUser1.first.toStdString() << ":admin:" << kUser1.second.toStdString() << '\n';

        // Missing first ":".
        input << kUser2.first.toStdString() << "owner:" << kUser2.second.toStdString() << '\n';

        // Missing second ":".
        input << kUser3.first.toStdString() << ":manager" << kUser3.second.toStdString() << '\n';

        // Missing username.
        input << ":basic_user:" << kUser4.second.toStdString() << '\n';

        return input;
    }

    void whenCreateAuthenticationProviderWithValidInputStream()
    {
        m_authenticationProvider = std::make_unique <HtDigestAuthenticationProvider>(
            getDefaultInputStream());
    }

    void whenCreateAuthenticationProviderWithMalformedInputStream()
    {
        m_authenticationProvider = std::make_unique <HtDigestAuthenticationProvider>(
            getMalformedInputStream());
    }

    void andPasswordLookupSucceeds(const std::pair<nx::String, nx::String>& user)
    {
        m_authenticationProvider->getPasswordByUserName(user.first,
            [user](PasswordLookupResult result)
            {
                ASSERT_EQ(result.code, PasswordLookupResult::Code::ok);
                ASSERT_EQ(result.type, PasswordLookupResult::DataType::ha1);
                ASSERT_TRUE(result.ha1().is_initialized());
                ASSERT_EQ(*result.ha1(), user.second);
            });
    }

    void andPasswordLookupFails(const std::pair<nx::String, nx::String>& user)
    {
        m_authenticationProvider->getPasswordByUserName(user.first,
            [](PasswordLookupResult result)
            {
                ASSERT_NE(result.code, PasswordLookupResult::Code::ok);
            });
    }

    void andValidPasswordLookupsSucceed()
    {
        andPasswordLookupSucceeds(kUser1);
    }

    void andAllPasswordLookupsSucceed()
    {
        andPasswordLookupSucceeds(kUser1);
        andPasswordLookupSucceeds(kUser2);
        andPasswordLookupSucceeds(kUser3);
        andPasswordLookupSucceeds(kUser4);
    }

    void andPasswordLookupFailsForUnknownUser()
    {
        andPasswordLookupFails(kUnknownUser);
    }

    void andAllMalformedUserPasswordCombinationLookupsFail()
    {
        andPasswordLookupFails(kUser2);
        andPasswordLookupFails(kUser3);
        andPasswordLookupFails(kUser4);
    }

protected:
    std::unique_ptr <HtDigestAuthenticationProvider> m_authenticationProvider;
};

TEST_F(HtDigest, htdigest_authentication_provider_provides_passwords_for_all_known_users)
{
    whenCreateAuthenticationProviderWithValidInputStream();

    andAllPasswordLookupsSucceed();
}

TEST_F(HtDigest, htdigest_authentication_provider_rejects_password_lookup_for_unknown_user)
{
    whenCreateAuthenticationProviderWithValidInputStream();

    andPasswordLookupFailsForUnknownUser();
}

TEST_F(HtDigest,
    htdigest_authenticatin_provider_provides_password_for_valid_user_password_combination_after_loading_malformed_input_stream)
{
    whenCreateAuthenticationProviderWithMalformedInputStream();

    andValidPasswordLookupsSucceed();
}

TEST_F(HtDigest,
    htdigest_authentication_provider_rejects_password_lookups_for_malformed_user_password_combinations_after_loading_malformed_input_stream)
{
    whenCreateAuthenticationProviderWithMalformedInputStream();

    andAllMalformedUserPasswordCombinationLookupsFail();
}

} // namespace nx::network::http::server::test