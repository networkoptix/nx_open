#include <gtest/gtest.h>
#include <nx/update/info/update_request_data.h>

namespace nx {
namespace update {
namespace info {
namespace test {

TEST(OsVersion, match)
{
    ASSERT_TRUE(ubuntuX64().matches("linux.x64_ubuntu"));
    ASSERT_TRUE(ubuntuX86().matches("linux.x86_ubuntu"));
    ASSERT_TRUE(windowsX64().matches("windows.x64_winxp"));
    ASSERT_TRUE(windowsX86().matches("windows.x86_winxp"));
    ASSERT_TRUE(armBpi().matches("linux.arm_bpi"));
    ASSERT_TRUE(armRpi().matches("linux.arm_rpi"));
    ASSERT_TRUE(armBananapi().matches("linux.arm_bananapi"));
}

TEST(OsVersion, noMatch)
{
    ASSERT_FALSE(ubuntuX64().matches("linux.arm_rpi"));
    ASSERT_FALSE(ubuntuX86().matches("linux.x64_ubuntu"));
    ASSERT_FALSE(windowsX64().matches("linux.arm_bananapi"));
    ASSERT_FALSE(windowsX86().matches("linux.x86_ubuntu"));
    ASSERT_FALSE(armBpi().matches("linux.arm_rpi"));
    ASSERT_FALSE(armRpi().matches("linux.arm_bpi"));
    ASSERT_FALSE(armBananapi().matches("linux.arm_bnanapi"));
}

} // namespace test
} // namespace info
} // namespace update
} // namespace nx