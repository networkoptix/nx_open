// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include <gtest/gtest.h>
#include <nx/utils/scrypt.h>

namespace nx::text {

using Args = std::tuple<std::string, std::string, std::string>;

TEST(SCrypt, Defaults)
{
    EXPECT_TRUE(scrypt::Options{}.isValid());

    for (const auto& [password, salt, expectedResult]: std::vector<Args>{
        Args{"password", "salt", "1effd93afcf2b28964026631bf4362b0e5ed83cbd5f326b72eb687bfbc7ac567"},
        Args{"HelloWorld", "salt", "7803923e3350095a389ea03097ebcd15650442117eb8711d042706d964f54d21"},
        Args{"hello world", "salt", "7b5dc3a4d7be4d8d0d5d232a55f3ed7950b4c5357c2ae73d7b4cb03ddc16fe15"},
        Args{"hello world", "thisIsBigSalt", "caeea7dec2c90b6823548fff42377de44a7502da6af96179edd02c300d65dcea"},
    })
    {
        EXPECT_EQ(scrypt::encodeOrThrow(password, salt), expectedResult)
            << password << " -- " << salt;
    }
}

TEST(SCrypt, Strong)
{
    scrypt::Options options;
    options.N = 128 * 1024;
    options.r = 8;
    options.p = 1;
    options.keySize = 16;
    EXPECT_TRUE(options.isValid());

    for (const auto& [password, salt, expectedResult]: std::vector<Args>{
        Args{"password", "salt", "621b282083cea28c49ab3673360283cf"},
        Args{"HelloWorld", "salt", "74fe2b1faeaa9fe2e62697f326154eb1"},
        Args{"hello world", "salt", "bb1391d1f8314540f7f4b18eb25a314a"},
        Args{"hello world", "thisIsBigSalt", "c682e3f3cd7eecd73ef6f477ab0482f2"},
    })
    {
        EXPECT_EQ(scrypt::encodeOrThrow(password, salt, options), expectedResult)
            << password << " -- " << salt;
    }
}

TEST(SCrypt, Exceptions)
{
    #define EXPECT_EXCEPTION(SETUP, WHAT) \
        try \
        { \
            scrypt::Options options; \
            options.SETUP; \
            EXPECT_FALSE(options.isValid()); \
            \
            scrypt::encodeOrThrow("password", "salt", options); \
            FAIL() << "Expected exception: " << WHAT; \
        } \
        catch (scrypt::Exception& e) \
        { \
            EXPECT_EQ(e.what(), std::string(WHAT)); \
        }

    EXPECT_EXCEPTION(N = 100 * 1024, "Unable to set SCrypt N (CPU/memory cost)");
    EXPECT_EXCEPTION(r = 0, "Unable to set SCrypt r (block size)");
    EXPECT_EXCEPTION(p = 0, "Unable to set SCrypt p (parallelization)");
}

} // namespace nx::text
