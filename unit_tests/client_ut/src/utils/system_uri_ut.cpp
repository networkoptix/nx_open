#include <gtest/gtest.h>

#include <utils/system_uri.h>

/** Initial test. Check if default list is empty. */
TEST( QnSystemUriTest, init )
{
    QnSystemUri uri;
    ASSERT_TRUE(uri.isNull());
}