#include <gtest/gtest.h>

#include <nx/network/http/auth_tools.h>
#include <nx/utils/log/log.h>

namespace nx {
namespace network {
namespace http {
namespace test {

TEST(HttpAuthDigest, partialResponse_test)
{
    const char username[] = "hren";
    const char realm[] = "networkoptix.com";
    const char password[] = "123";
    const char nonce[] = "31-byte_long_nonce-------------";
    const char nonceTrailer[] = "random_nonce_trailer";
    const char method[] = "GET";
    const char uri[] = "/get_all";

    const auto ha1 = nx::network::http::calcHa1(username, realm, password);
    const auto ha2 = nx::network::http::calcHa2(method, uri);
    const auto response = nx::network::http::calcResponse(
        ha1,
        QByteArray(nonce)+QByteArray(nonceTrailer),
        ha2);

    const auto partialResponce = nx::network::http::calcIntermediateResponse(ha1, nonce);
    const auto responseFromPartial = nx::network::http::calcResponseFromIntermediate(
        partialResponce,
        sizeof(nonce) - 1,
        nonceTrailer,
        ha2);

    ASSERT_EQ(responseFromPartial, response);
}

void testCalcDigestResponse(
    const StringType& method,
    const StringType& userName,
    const boost::optional<StringType>& userPassword,
    const boost::optional<BufferType>& predefinedHA1,
    const StringType& algorithm = {})
{
    header::WWWAuthenticate authHeader(header::AuthScheme::digest);
    authHeader.params.insert("realm", "test@networkoptix.com");
    authHeader.params.insert("qop", "auth");
    authHeader.params.insert("nonce", "dcd98b7102dd2f0e8b11d0f600bfb0c093");
    if (!algorithm.isEmpty())
        authHeader.params.insert("algorithm", algorithm);

    header::DigestAuthorization digestHeader;
    ASSERT_TRUE(calcDigestResponse(
        method, userName, userPassword, predefinedHA1, "/some/uri",
        authHeader, &digestHeader));

    ASSERT_TRUE(validateAuthorization(
        method, userName, userPassword, predefinedHA1, digestHeader));
}

TEST(HttpAuthDigest, calcDigestResponse)
{
    for (const StringType& method: {"GET", "POST", "PUT"})
    {
        for (const StringType& user: {"user", "admin"})
        {
            typedef boost::optional<StringType> OptionalStr;
            for (const OptionalStr& password: {OptionalStr(), OptionalStr("admin")})
            {
                for (const StringType& algorithm: {"", "MD5", "SHA-256"})
                {
                    NX_LOGX(lm("Test method='%1', user='%2', password='%3', algorithm='%4'")
                        .args(method, user, password, algorithm), cl_logDEBUG1);

                    testCalcDigestResponse(method, user, password, boost::none, algorithm);
                }
            }
        }
    }
}

} // namespace test
} // namespace nx
} // namespace network
} // namespace http
