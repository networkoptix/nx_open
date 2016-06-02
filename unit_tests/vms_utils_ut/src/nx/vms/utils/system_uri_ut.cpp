#include <gtest/gtest.h>

#include <nx/vms/utils/system_uri.h>

using namespace nx::vms;

/** Initial test. Check if default list is empty. */
TEST( SystemUriTest, init )
{
    SystemUri uri;
    ASSERT_TRUE(uri.isNull());
}