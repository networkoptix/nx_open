// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include <gtest/gtest.h>

#include <nx/network/jose/jwk.h>

namespace nx::network::jwk::test {

class JoseJwk:
    public ::testing::Test
{

};

TEST_F(JoseJwk, generate_keypair)
{
    const auto kp = generateKeyPairForSigning(Algorithm::RS256);
    ASSERT_TRUE(kp.has_value());

    ASSERT_EQ("RSA", kp->pub.kty());
    ASSERT_EQ(Use::sig, kp->pub.use());
    ASSERT_EQ(std::vector<KeyOp>({KeyOp::verify}), kp->pub.keyOps());
    ASSERT_EQ("RS256", kp->pub.alg());
    ASSERT_FALSE(kp->pub.kid().empty());
    ASSERT_TRUE(kp->pub.contains("n"));
    ASSERT_FALSE(kp->pub.get<std::string>("n")->empty());
    ASSERT_TRUE(kp->pub.contains("e"));
    ASSERT_FALSE(kp->pub.get<std::string>("e")->empty());

    ASSERT_EQ("RSA", kp->priv.kty());
    ASSERT_EQ(Use::sig, kp->priv.use());
    ASSERT_EQ(std::vector<KeyOp>({KeyOp::sign}), kp->priv.keyOps());
    ASSERT_EQ("RS256", kp->priv.alg());
    ASSERT_EQ(kp->pub.kid(), kp->priv.kid());
    ASSERT_EQ(kp->pub.get<std::string>("n"), kp->priv.get<std::string>("n"));
    ASSERT_EQ(kp->pub.get<std::string>("e"), kp->priv.get<std::string>("e"));
    ASSERT_TRUE(kp->priv.contains("d"));
    ASSERT_FALSE(kp->priv.get<std::string>("d")->empty());
}

TEST_F(JoseJwk, sign_and_verify_message)
{
    const auto kp = generateKeyPairForSigning(Algorithm::RS256);
    ASSERT_TRUE(kp.has_value());

    const std::string message = "Hello, world!";
    const auto& signResult = sign(message, kp->priv);
    ASSERT_TRUE(signResult.has_value()) << signResult.error();
    ASSERT_FALSE(signResult->signature.empty());
    ASSERT_EQ("RS256", signResult->alg);

    ASSERT_TRUE(verify(message, signResult->signature, "RS256", kp->pub));
}

TEST_F(JoseJwk, altered_message_does_not_pass_verification)
{
    const auto keyPair = generateKeyPairForSigning(Algorithm::RS256);
    ASSERT_TRUE(keyPair.has_value());

    const std::string message = "Hello, world!";
    const auto& signResult = sign(message, keyPair->priv);
    ASSERT_TRUE(signResult.has_value()) << signResult.error();

    ASSERT_FALSE(verify(message + "1", signResult->signature, "RS256", keyPair->pub));
}

} // namespace nx::network::jwk::test
