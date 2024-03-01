// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include <gtest/gtest.h>

#include <nx/network/jose/jwt.h>

namespace nx::network::jwt::test {

class JoseJwt:
    public ::testing::Test
{
protected:
    virtual void SetUp() override
    {
        auto kp = jwk::generateKeyPairForSigning(jwk::Algorithm::RS256);
        ASSERT_TRUE(kp.has_value());
        m_keyPair = kp.value();
    }

    ClaimSet givenRandomClaimSet()
    {
        ClaimSet claimSet;
        claimSet.setIss("issuer");
        claimSet.setSub("subject");
        claimSet.setAud("audience");
        claimSet.setExp(std::chrono::seconds(1234567890));
        claimSet.setNbf(std::chrono::seconds(1234567890));
        claimSet.setIat(std::chrono::seconds(1234567890));
        return claimSet;
    }

    std::string encodeAndSignToken(const ClaimSet& claimSet, const jwk::Key& key)
    {
        const auto result = encodeAndSign(claimSet, key);
        EXPECT_TRUE(result);
        return result->encodedAndSignedToken;
    }

    std::string givenRandomEncodedToken(const jwk::Key& key)
    {
        const auto claimSet = givenRandomClaimSet();
        return encodeAndSignToken(claimSet, key);
    }

    Token whenDecodeToken(const std::string& encodedToken)
    {
        const auto result = decodeToken(encodedToken);
        EXPECT_TRUE(result);
        return *result;
    }

    void assertIsValidEncodedJwtToken(const std::string& encodedToken)
    {
        const auto [parts, count] = nx::utils::split_n<3>(encodedToken, '.');
        ASSERT_EQ(3, count);

        nx::reflect::DeserializationResult result;

        jws::Header header;
        std::tie(header, result) = nx::reflect::json::deserialize<jws::Header>(
            nx::utils::fromBase64Url(parts[0]));
        ASSERT_TRUE(result.success);
        ASSERT_EQ(kJwtTokenType, header.typ);
        ASSERT_EQ(m_keyPair.priv.alg(), header.alg);

        ClaimSet claims;
        std::tie(claims, result) = nx::reflect::json::deserialize<ClaimSet>(
            nx::utils::fromBase64Url(parts[1]));
        ASSERT_TRUE(result.success);
        ASSERT_TRUE(claims.contains("iss"));
        ASSERT_TRUE(claims.contains("sub"));

        // signature is non-empty.
        ASSERT_FALSE(parts[2].empty());
    }

    jwk::KeyPair keyPair() const { return m_keyPair; }

private:
    jwk::KeyPair m_keyPair;
};

TEST_F(JoseJwt, generate_and_sign_token)
{
    const auto claimSet = givenRandomClaimSet();
    const auto encodedToken = encodeAndSignToken(claimSet, keyPair().priv);
    assertIsValidEncodedJwtToken(encodedToken);
}

TEST_F(JoseJwt, verify_signed_token)
{
    const auto encodedToken = givenRandomEncodedToken(keyPair().priv);

    // Decoding token before signature verification because we need to extract algorithm, iss, kid
    // to verify signature.
    const auto decodedToken = whenDecodeToken(encodedToken);

    // In real life, we would match decodedToken.payoad.iss() and decodedToken.payoad.kid() to
    // keyPair().pub.
    ASSERT_TRUE(verifyToken(encodedToken, keyPair().pub));
}

TEST_F(JoseJwt, token_with_altered_signature_does_not_pass_verification)
{
    auto encodedToken = givenRandomEncodedToken(keyPair().priv);

    // Alter the signature.
    encodedToken[encodedToken.size() - 5]++;

    // Token can still be decoded fine.
    const auto decodedToken = whenDecodeToken(encodedToken);
    // But verification must fail.
    ASSERT_FALSE(verifyToken(encodedToken, keyPair().pub));
}

// Test that claims are preserved between encode/decode.
TEST_F(JoseJwt, custom_token_claims_are_preserved_between_encode_decode)
{
    const std::map<std::string, std::string> customClaims = {
        {"custom1", "value1"},
        {"http://example.com/custom2", "value2"}
    };

    auto claimSet = givenRandomClaimSet();
    for (const auto& [k, v]: customClaims)
        claimSet.set(k, v);

    const auto encodedToken = encodeAndSignToken(claimSet, keyPair().priv);

    const auto decodedToken = whenDecodeToken(encodedToken);
    for (const auto& [k, v]: customClaims)
        ASSERT_EQ(v, decodedToken.payload.get<std::string>(k).value_or(""));
}

} // namespace nx::network::jwt::test
