// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include <gtest/gtest.h>

#include <nx/reflect/json/raw_json_text.h>
#include <nx/utils/random.h>

#include <nx/network/jose/jws.h>

namespace nx::network::jws::test {

struct Payload
{
    std::string foo;

    bool operator==(const Payload&) const = default;
};

NX_REFLECTION_INSTRUMENT(Payload, (foo))

class JoseJws:
    public ::testing::Test
{
protected:
    jwk::KeyPair givenKeyPair()
    {
        const auto keyPair = jwk::generateKeyPairForSigning(jwk::Algorithm::RS256);
        EXPECT_TRUE(keyPair.has_value());
        return *keyPair;
    }

    Token<Payload> givenRandomToken(const jwk::Key& key)
    {
        const Payload payload{.foo = "bar"};
        const Token<Payload> token{
            .header = Header{.typ = "JWT", .alg = key.alg(), .kid = key.kid()}, .payload = payload};
        return token;
    }

    TokenEncodeResult<Payload> whenEncodeAndSignToken(
        const std::string& typ, const Payload& payload, const jwk::Key& key)
    {
        auto result = encodeAndSign(typ, payload, key);
        EXPECT_TRUE(result);
        return *result;
    }
};

static constexpr char kRfc7515AppendixA11_signedToken[] =
    "eyJ0eXAiOiJKV1QiLA0KICJhbGciOiJIUzI1NiJ9"
    "."
    "eyJpc3MiOiJqb2UiLA0KICJleHAiOjEzMDA4MTkzODAsDQogImh0dHA6Ly9leGFt"
    "cGxlLmNvbS9pc19yb290Ijp0cnVlfQ"
    "."
    "dBjftJeZ4CVP-mB92K27uhbUJU1p1r_wW1gFWFOEjXk";

// Uses values from https://datatracker.ietf.org/doc/html/rfc7515#appendix-A.1.1.
TEST_F(JoseJws, sign_token_rfc7515_example)
{
    // Note: I cannot reproduce the example using jws functions since rfc example inserts additional
    // line breaks into JSON which cannot be replicated using nx::reflect::json.
    // So, reproducing the example using jwk API.

    const std::string jsonHeader = "{\"typ\":\"JWT\",\r\n \"alg\":\"HS256\"}";
    const std::string jsonPayload =
        "{\"iss\":\"joe\",\r\n \"exp\":1300819380,\r\n \"http://example.com/is_root\":true}";

    jwk::Key key;
    key.setKty("oct");
    key.set("k", "AyM1SysPpbyDfgZld3umj1qzKObwVMkoqQ-EstJQLr_T-1qS0gZH75aKtMN3Yj0iPS4hcgUuTwjAzZr1Z9CAow");
    key.setKeyOps({jwk::KeyOp::sign, jwk::KeyOp::verify});
    key.setAlg("HS256");
    const auto base64EncodedToken =
        nx::utils::toBase64Url(jsonHeader) + "." + nx::utils::toBase64Url(jsonPayload);
    const auto result = jwk::sign(base64EncodedToken, key);
    ASSERT_TRUE(result.has_value()) << result.error();

    const auto signedToken = base64EncodedToken + "." + result->signature;
    ASSERT_EQ(kRfc7515AppendixA11_signedToken, signedToken);
}

TEST_F(JoseJws, verify_token)
{
    const auto kp = givenKeyPair();
    const auto expectedToken = givenRandomToken(kp.priv);

    auto signResult = encodeAndSign("JWT", expectedToken.payload, kp.priv);
    ASSERT_TRUE(signResult.has_value());
    ASSERT_EQ(expectedToken, signResult->token);

    // It is a common practice to decode token before verifying signature to get the issuer and
    // key id.
    auto decodedToken = decodeToken<Payload>(signResult->encodedAndSignedToken);
    ASSERT_TRUE(decodedToken.has_value());
    ASSERT_EQ(expectedToken, *decodedToken);

    // Now, verifying the token.
    ASSERT_TRUE(verifyToken(signResult->encodedAndSignedToken, kp.pub));
}

TEST_F(JoseJws, altered_token_cannot_be_verified)
{
    const auto kp = givenKeyPair();
    const auto expectedToken = givenRandomToken(kp.priv);

    TokenEncodeResult signResult = whenEncodeAndSignToken(
        expectedToken.header.typ, expectedToken.payload, kp.priv);

    // Alter the token.
    ++nx::utils::random::choice(signResult.encodedAndSignedToken);

    ASSERT_FALSE(verifyToken(signResult.encodedAndSignedToken, kp.pub));
}

TEST_F(JoseJws, decoding_trash_does_not_crash)
{
    const auto kp = givenKeyPair();

    std::string trash(nx::utils::random::number<int>(1, 1000), '\0');
    std::generate(trash.begin(), trash.end(),
        []() { return rand() % std::numeric_limits<char>::max(); });

    auto decodedToken = decodeToken<Payload>(trash);
    // If decoding still completes somehow, the verification MUST never pass.
    ASSERT_FALSE(decodedToken.has_value() && verifyToken(trash, kp.pub));
}

} // namespace nx::network::jws::test
