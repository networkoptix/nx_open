// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include <gtest/gtest.h>

#include <nx/reflect/json/raw_json_text.h>
#include <nx/utils/random.h>
#include <nx/utils/time.h>

#include <nx/network/jose/jws.h>
#include <nx/network/jose/jwt.h>

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

    std::string tmp(signResult.encodedAndSignedToken);

    // Alter the token.
    // NOTE: Never altering the last character since it may be a base64 padding modifying which
    // does not actually corrupts the encoded message.
    const int pos = nx::utils::random::number<int>(0, tmp.size() - 2);
    ++tmp[pos];

    ASSERT_FALSE(verifyToken(tmp, kp.pub));
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

//-------------------------------------------------------------------------------------------------

class JoseJwsLoadTest:
    public JoseJws
{
protected:
    template<typename Key>
    nx::utils::expected<nx::network::jws::TokenEncodeResult<jwt::ClaimSet>, std::string>
        generateToken(const Key& key)
    {
        jwt::ClaimSet claimSet;
        claimSet.setIss("cdb");
        claimSet.setSub("test@example.com");
        claimSet.setIat(
            std::chrono::duration_cast<std::chrono::seconds>(nx::utils::utcTime().time_since_epoch()));
        claimSet.setAud("/ cloudSystemId=*");
        claimSet.setIat(std::chrono::duration_cast<std::chrono::seconds>(nx::utils::utcTime().time_since_epoch()));
        claimSet.setExp(std::chrono::duration_cast<std::chrono::seconds>(
            nx::utils::utcTime().time_since_epoch() + std::chrono::minutes(10)));

        nx::utils::expected<nx::network::jws::TokenEncodeResult<jwt::ClaimSet>, std::string>
            tokenEncodeResult = nx::network::jws::encodeAndSign(
                nx::network::jwt::kJwtTokenType, claimSet, key);

        return tokenEncodeResult;
    }

    template<typename Func>
    void measurePerformance(
        const std::string& testName,
        Func f)
    {
        static constexpr auto testDuration = std::chrono::seconds(3);

        const auto t0 = std::chrono::steady_clock::now();
        std::chrono::steady_clock::time_point t1;
        int cnt = 0;
        while ((t1 = std::chrono::steady_clock::now()) < t0 + testDuration)
        {
            f();
            cnt++;
        }

        const int rps = (cnt * 1000) / std::chrono::duration_cast<std::chrono::milliseconds>(t1 - t0).count();

        std::cout<<testName<<" rate is "<< rps << " tokens per second" <<std::endl;
    }
};

TEST_F(JoseJwsLoadTest, DISABLED_token_preparation_and_signing)
{
    const auto jwk = nx::network::jwk::generateKeyPairForSigning(nx::network::jwk::Algorithm::RS256);
    ASSERT_TRUE(jwk);

    const auto parsedKey = nx::network::jwk::parseKey(jwk->priv);
    ASSERT_TRUE(parsedKey);

    measurePerformance(
        "Token preparation and signing",
        [this, &parsedKey]()
        {
            const auto token = generateToken(*parsedKey);
            ASSERT_TRUE(token);
        });
}

TEST_F(JoseJwsLoadTest, DISABLED_verifying)
{
    const auto jwk = nx::network::jwk::generateKeyPairForSigning(nx::network::jwk::Algorithm::RS256);
    ASSERT_TRUE(jwk);

    const auto parsedPubKey = nx::network::jwk::parseKey(jwk->pub);
    ASSERT_TRUE(parsedPubKey);

    const auto token = generateToken(jwk->priv);
    ASSERT_TRUE(token);

    measurePerformance(
        "Token verification",
        [&token, &parsedPubKey]()
        {
            ASSERT_TRUE(verifyToken(token->encodedAndSignedToken, *parsedPubKey));
        });
}

} // namespace nx::network::jws::test
