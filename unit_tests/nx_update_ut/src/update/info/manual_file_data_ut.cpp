#include <gtest/gtest.h>
#include <nx/update/info/manual_file_data.h>

namespace nx {
namespace update {
namespace info {
namespace test {

namespace {

static void assertData(
    const ManualFileData& data, const QString& file, const OsVersion& osVersion,
    const QnSoftwareVersion& nxVersion, bool isClient)
{
    ASSERT_EQ(file, data.file);
    ASSERT_EQ(osVersion, data.osVersion);
    ASSERT_EQ(nxVersion, data.nxVersion);
    ASSERT_EQ(isClient, data.isClient);
}

} // namespace

TEST(ManualFileData, ConversionFromFileName)
{
    assertData(
        ManualFileData::fromFileName("nxwitness-server_update-3.1.0.16975-rpi.zip"),
        "nxwitness-server_update-3.1.0.16975-rpi.zip", armRpi(), QnSoftwareVersion("3.1.0.16975"),
        false);

    assertData(
        ManualFileData::fromFileName("nxwitness-client_update-3.1.0.16975-mac.zip"),
        "nxwitness-client_update-3.1.0.16975-mac.zip", macosx(), QnSoftwareVersion("3.1.0.16975"),
        true);

    assertData(
        ManualFileData::fromFileName("nxwitness-client_update-4.0.0.982-linux64-beta-test.zip"),
        "nxwitness-client_update-4.0.0.982-linux64-beta-test.zip", ubuntuX64(), QnSoftwareVersion("4.0.0.982"),
        true);

    assertData(
        ManualFileData::fromFileName("nxwitness-server_update-4.0.0.982-win64-beta-test.zip"),
        "nxwitness-server_update-4.0.0.982-win64-beta-test.zip", windowsX64(), QnSoftwareVersion("4.0.0.982"),
        false);
}

} // namespace test
} // namespace info
} // namespace update
} // namespace nx
