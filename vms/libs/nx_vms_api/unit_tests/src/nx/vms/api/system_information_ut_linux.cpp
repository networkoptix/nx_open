#include <array>
#include <gtest/gtest.h>
#include <nx/vms/api/helpers/system_information_helper_linux.h>

namespace nx::vms::api::test {

TEST(ubuntuVersionFromOsReleaseContents, success)
{
    const auto validOsReleaseFiles = {
R"rel(
NAME="Linux Mint"
VERSION="18.3 (Sylvia)"
ID=linuxmint
ID_LIKE=ubuntu
PRETTY_NAME="Linux Mint 18.3"
VERSION_ID="18.3"
HOME_URL="http://www.linuxmint.com/"
SUPPORT_URL="http://forums.linuxmint.com/"
BUG_REPORT_URL="http://bugs.launchpad.net/linuxmint/"
VERSION_CODENAME=sylvia
UBUNTU_CODENAME=xenial
)rel",

R"rel(
NAME="Ubuntu"
VERSION="16.04.5 LTS (Xenial Xerus)"
ID=ubuntu
ID_LIKE=debian
PRETTY_NAME="Ubuntu 16.04.5 LTS"
VERSION_ID="16.04"
HOME_URL="http://www.ubuntu.com/"
SUPPORT_URL="http://help.ubuntu.com/"
BUG_REPORT_URL="http://bugs.launchpad.net/ubuntu/"
VERSION_CODENAME=xenial
UBUNTU_CODENAME=xenial
)rel",

R"rel(
NAME="Ubuntu"
VERSION="18.04.1 LTS (Bionic Beaver)"
ID=ubuntu
ID_LIKE=debian
PRETTY_NAME="Ubuntu 18.04.1 LTS"
VERSION_ID="18.04"
HOME_URL="https://www.ubuntu.com/"
SUPPORT_URL="https://help.ubuntu.com/"
BUG_REPORT_URL="https://bugs.launchpad.net/ubuntu/"
PRIVACY_POLICY_URL="https://www.ubuntu.com/legal/terms-and-policies/privacy-policy"
VERSION_CODENAME=bionic
UBUNTU_CODENAME=bionic
)rel"
    };

    for (const auto &contents: validOsReleaseFiles)
    {
        ASSERT_TRUE(
            ubuntuVersionFromOsReleaseContents(contents) == "16.04"
            || ubuntuVersionFromOsReleaseContents(contents) == "18.04");
    }
}

TEST(ubuntuVersionFromOsReleaseContents, failure)
{
}

} // namespace nx::vms::api::test

