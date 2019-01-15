#include <array>
#include <gtest/gtest.h>
#include <nx/vms/api/helpers/system_information_helper_linux.h>

namespace nx::vms::api::test {

TEST(ubuntuVersion, fromCodeName_success)
{
    const auto ubuntuBasedOsReleaseFile =
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
)rel";

const auto ubuntu1604OsReleaseFile =
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
)rel";

const auto ubuntu1804OsReleaseFile =
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
)rel";

    const auto codenameKeyName = "ubuntu_codename";
    ASSERT_EQ(
        ubuntuVersionFromCodeName(
            osReleaseContentsValueByKey(ubuntuBasedOsReleaseFile, codenameKeyName)),
        "16.04");

    ASSERT_EQ(
        ubuntuVersionFromCodeName(
            osReleaseContentsValueByKey(ubuntu1604OsReleaseFile, codenameKeyName)),
        "16.04");

    ASSERT_EQ(
        ubuntuVersionFromCodeName(
            osReleaseContentsValueByKey(ubuntu1804OsReleaseFile, codenameKeyName)),
        "18.04");
}

TEST(ubuntuVersion, fromCodeName_failure)
{
    const auto invalidOsReleaseFiles = {
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
UBUNTUCODENAME=xenial
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
UBUNTU_CODENAME bionic
)rel"
    };

    for (const auto &contents: invalidOsReleaseFiles)
    {
        ASSERT_TRUE(ubuntuVersionFromCodeName(
            osReleaseContentsValueByKey(contents, "ubuntu_codename")).isEmpty());
    }
}

TEST(ubuntuVersion, osReleaseContentsValueByKey_success)
{
    const auto osReleaseContents =
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
)rel";

    ASSERT_EQ("Linux Mint 18.3", osReleaseContentsValueByKey(osReleaseContents, "pretty_name"));
    ASSERT_EQ("18.3", osReleaseContentsValueByKey(osReleaseContents, "VeRSiON_ID"));
    ASSERT_EQ("sylvia", osReleaseContentsValueByKey(osReleaseContents, "VERSION_CODENAME"));
}

TEST(ubuntuVersion, ubuntuCodenameIsAbsent)
{
    const auto ubuntu1404OsReleaseContentsWithoutUbuntuCodename =
R"rel(
NAME="Ubuntu"
VERSION="14.04.4 LTS, Trusty Tahr"
ID=ubuntu
ID_LIKE=debian
PRETTY_NAME="Ubuntu 14.04.4 LTS"
VERSION_ID="14.04"
HOME_URL="http://www.ubuntu.com/"
SUPPORT_URL="http://help.ubuntu.com/"
BUG_REPORT_URL="http://bugs.launchpad.net/ubuntu/"
)rel";

    const auto codenameValue = osReleaseContentsValueByKey(
        ubuntu1404OsReleaseContentsWithoutUbuntuCodename,
        "ubuntu_codename");
    ASSERT_TRUE(ubuntuVersionFromCodeName(codenameValue).isEmpty());
    const auto versionIdValue = osReleaseContentsValueByKey(
        ubuntu1404OsReleaseContentsWithoutUbuntuCodename,
        "version_id");
    ASSERT_EQ("14.04", versionIdValue);
}

} // namespace nx::vms::api::test

