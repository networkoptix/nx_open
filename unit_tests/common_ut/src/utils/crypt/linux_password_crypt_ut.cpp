/**********************************************************
* Oct 21, 2015
* a.kolesnikov
***********************************************************/

#include <gtest/gtest.h>

#include <nx/utils/crypt/linux_passwd_crypt.h>


TEST(linuxCrypt, generateSalt)
{
    for (int i = 0; i < 10000; ++i)
    {
        const auto salt = generateSalt(LINUX_CRYPT_SALT_LENGTH);
        ASSERT_TRUE(salt.indexOf('\0') == -1);
    }
}
