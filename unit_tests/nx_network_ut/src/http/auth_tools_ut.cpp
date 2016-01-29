/**********************************************************
* Sep 25, 2015
* a.kolesnikov
***********************************************************/

#include <nx/network/http/auth_tools.h>

#include <gtest/gtest.h>

#include <nx/network/http/auth_tools.h>


TEST(HttpAuthDigest, partialResponse_test)
{
    const char username[] = "hren";
    const char realm[] = "networkoptix.com";
    const char password[] = "123";
    const char nonce[] = "31-byte_long_nonce-------------";
    const char nonceTrailer[] = "random_nonce_trailer";
    const char method[] = "GET";
    const char uri[] = "/get_all";

    const auto ha1 = nx_http::calcHa1(username, realm, password);
    const auto ha2 = nx_http::calcHa2(method, uri);
    const auto response = nx_http::calcResponse(
        ha1,
        QByteArray(nonce)+QByteArray(nonceTrailer),
        ha2);

    const auto partialResponce = nx_http::calcIntermediateResponse(ha1, nonce);
    const auto responseFromPartial = nx_http::calcResponseFromIntermediate(
        partialResponce,
        sizeof(nonce) - 1,
        nonceTrailer,
        ha2);

    ASSERT_EQ(responseFromPartial, response);
}
