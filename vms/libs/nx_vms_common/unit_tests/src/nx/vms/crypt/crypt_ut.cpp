// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include <gtest/gtest.h>

#include <nx/vms/crypt/crypt.h>

namespace nx::vms::crypt::test {

TEST(Aes, makeKey)
{
    static const QLatin1String kPassword("Hello!");
    static const QByteArray kSalt1("salt1");
    static const QByteArray kSalt2("salt2");
    static const uint64_t kNonce = 54321;

    AesKey emptyKey;
    AesKey key128 = makeKey(kPassword, Algorithm::aes128, kSalt1);
    AesKey key128_v2 = makeKey(kPassword, Algorithm::aes128, kSalt1);
    AesKey key256 = makeKey(kPassword, Algorithm::aes256, kSalt1);
    AesKey key256_v2 = makeKey(kPassword, Algorithm::aes256, kSalt1);

    ASSERT_EQ(key128.cipherKey, key128_v2.cipherKey);
    ASSERT_EQ(key128.ivVect, key128_v2.ivVect);
    ASSERT_EQ(key256.cipherKey, key256_v2.cipherKey);
    ASSERT_EQ(key256.ivVect, key256_v2.ivVect);

    ASSERT_NE(key128.cipherKey, emptyKey.cipherKey);
    ASSERT_NE(key256.cipherKey, emptyKey.cipherKey);

    ASSERT_NE(key128.cipherKey, key256.cipherKey);
    ASSERT_NE(key128.ivVect, key256.ivVect);

    AesKey restoredKey128 = makeKey(kPassword, toEncryptionData(key128, kNonce));
    AesKey restoredKey256 = makeKey(kPassword, toEncryptionData(key256, kNonce));

    ASSERT_EQ(key128.cipherKey, restoredKey128.cipherKey);
    ASSERT_EQ(key256.cipherKey, restoredKey256.cipherKey);

    AesKey key128_salt2 = makeKey(kPassword, Algorithm::aes128, kSalt2);
    ASSERT_NE(key128.ivVect, key128_salt2.ivVect);
}

} // namespace nx::vms::crypt::test
