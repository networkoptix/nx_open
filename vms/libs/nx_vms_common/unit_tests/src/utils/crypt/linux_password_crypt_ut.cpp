// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include <gtest/gtest.h>

#include <nx/utils/crypt/linux_passwd_crypt.h>


TEST(linuxCrypt, generateSalt)
{
    for (int i = 0; i < 10000; ++i)
    {
        const auto salt = nx::utils::generateSalt(nx::utils::kLinuxCryptSaltLength);
        ASSERT_TRUE(salt.indexOf('\0') == -1);
    }
}
