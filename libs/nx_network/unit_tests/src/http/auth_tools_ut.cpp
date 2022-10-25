// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include <gtest/gtest.h>

#include <optional>

#include <nx/network/http/auth_tools.h>
#include <nx/utils/log/log.h>

namespace nx::network::http {

std::ostream& operator<<(std::ostream& s, const Credentials& credentials)
{
    const QString stringRepresentation = nx::format(
        "[user: %1, authtoken: %2, type: %3]",
        credentials.username,
        credentials.authToken.value,
        credentials.authToken.type);
    return s << stringRepresentation.toStdString();
}

namespace test {

TEST(HttpAuthDigest, partialResponse_test)
{
    const char username[] = "johndoe";
    const char realm[] = "example.com";
    const char password[] = "123";
    const char nonce[] = "31-byte_long_nonce-------------";
    const char nonceTrailer[] = "random_nonce_trailer";
    const auto method = Method::get;
    const char uri[] = "/get_all";

    const auto ha1 = nx::network::http::calcHa1(username, realm, password);
    const auto ha2 = nx::network::http::calcHa2(method, uri);
    const auto response = nx::network::http::calcResponse(
        ha1,
        std::string(nonce) + std::string(nonceTrailer),
        ha2);

    const auto partialResponse = nx::network::http::calcIntermediateResponse(ha1, nonce);
    const auto responseFromPartial = nx::network::http::calcResponseFromIntermediate(
        partialResponse,
        sizeof(nonce) - 1,
        nonceTrailer,
        ha2);

    ASSERT_EQ(responseFromPartial, response);
}

void testCalcDigestResponse(
    const Method& method,
    const std::string& userName,
    const std::optional<std::string>& userPassword,
    const std::optional<std::string>& predefinedHA1,
    const std::string& algorithm = {},
    const std::string& qualityOfProtection = {})
{
    header::WWWAuthenticate authHeader(header::AuthScheme::digest);
    authHeader.params.emplace("realm", "test@example.com");
    authHeader.params.emplace("qop", qualityOfProtection);
    authHeader.params.emplace("nonce", "dcd98b7102dd2f0e8b11d0f600bfb0c093");
    if (!algorithm.empty())
        authHeader.params.emplace("algorithm", algorithm);

    header::DigestAuthorization digestHeader;
    ASSERT_TRUE(calcDigestResponse(
        method, userName, userPassword, predefinedHA1, "/some/uri",
        authHeader, &digestHeader));

    ASSERT_TRUE(validateAuthorization(
        method, userName, userPassword, predefinedHA1, digestHeader));
}

TEST(HttpAuthDigest, calcDigestResponse)
{
    for (const auto& method: {Method::get, Method::post, Method::put})
    {
        for (const std::string user: {"user", "admin"})
        {
            using OptionalStr = std::optional<std::string>;
            for (const OptionalStr& password: {OptionalStr(), OptionalStr("admin")})
            {
                for (const std::string algorithm: {"", "MD5", "SHA-256"})
                {
                    for (const std::string qop: {"", "auth", "auth,auth-int", "auth-int,auth"})
                    {
                        NX_DEBUG(this,
                            "Test method='%1', user='%2', password='%3', algorithm='%4', qop='%5'",
                            method,
                            user,
                            password,
                            algorithm,
                            qop);

                        testCalcDigestResponse(method, user, password, std::nullopt, algorithm, qop);
                    }
                }
            }
        }
    }
}

namespace {

static const std::string kUser("user");
static const std::string kPassword("password");
static const std::string kHa1("ha1");
static const std::string kToken("token");

} // namespace

TEST(DeserializeCredentials, empty)
{
    SerializableCredentials empty;
    ASSERT_EQ(Credentials(empty), Credentials());
}

TEST(DeserializeCredentials, tokenOnly)
{
    SerializableCredentials tokenOnly;
    tokenOnly.token = kToken;
    ASSERT_EQ(Credentials(tokenOnly), Credentials({}, BearerAuthToken(kToken)));
}

TEST(DeserializeCredentials, passwordOnlyIsInvalid)
{
    SerializableCredentials passwordOnly;
    passwordOnly.password = kPassword;
    ASSERT_EQ(Credentials(passwordOnly), Credentials());
}

TEST(DeserializeCredentials, ha1OnlyIsInvalid)
{
    SerializableCredentials ha1Only;
    ha1Only.ha1 = kHa1;
    ASSERT_EQ(Credentials(ha1Only), Credentials());
}

TEST(DeserializeCredentials, token)
{
    SerializableCredentials token;
    token.user = kUser;
    token.token = kToken;
    ASSERT_EQ(Credentials(token), Credentials(kUser, BearerAuthToken(kToken)));
}

TEST(DeserializeCredentials, password)
{
    SerializableCredentials password;
    password.user = kUser;
    password.password = kPassword;
    ASSERT_EQ(Credentials(password), Credentials(kUser, PasswordAuthToken(kPassword)));
}

TEST(DeserializeCredentials, ha1)
{
    SerializableCredentials ha1;
    ha1.user = kUser;
    ha1.ha1 = kHa1;
    ASSERT_EQ(Credentials(ha1), Credentials(kUser, Ha1AuthToken(kHa1)));
}

TEST(DeserializeCredentials, userOnly)
{
    SerializableCredentials userOnly;
    userOnly.user = kUser;
    ASSERT_EQ(Credentials(userOnly), Credentials(kUser, {}));
}

} // namespace test
} // namespace nx::network::http
