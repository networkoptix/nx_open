#include <common/common_module.h>
#include <common/static_common_module.h>

#include "nx/vms/client/desktop/system_update/multi_server_updates_widget.h"

#include "client_update_test_environment.h"

namespace {

const QString packageRawData = R"(
{
    "version": "4.0.0.28524",
    "cloudHost": "nxvms.com",
    "releaseNotesUrl": "http://www.networkoptix.com/all-nx-witness-release-notes",
    "description": "",
    "eulaLink": "eula-1.html",
    "eulaVersion": 1,
    "packages": [
        {
            "component": "server",
            "platform": "windows_x64",
            "file": "nxwitness-server_update-4.0.0.28524-win64-beta-prod.zip",
            "size": 89479542,
            "md5": "88d9abcaae8ddf076fa1fe9cc829016a"
        },
        {
            "component": "client",
            "platform": "windows_x64",
            "file": "nxwitness-client_update-4.0.0.28524-win64-beta-prod.zip",
            "size": 99609610,
            "md5": "bd823350defd778c9990b908d90f92aa"
        }
    ]
})";

const QString packagesForSystemSupportTest = R"(
{
    "version": "4.0.0.29069",
    "cloudHost": "cloud-test.hdw.mx",
    "releaseNotesUrl": "http://www.networkoptix.com/all-nx-witness-release-notes",
    "description": "",
    "eulaLink": "eula-1.html",
    "eulaVersion": 1,
    "packages": [
        {
            "component": "server",
            "platform": "linux_x64",
            "variants": [
                {
                    "name": "ubuntu",
                    "minimumVersion": "18.04"
                }
            ],
            "file": "nxwitness-server_update-4.0.0.29069-linux64-beta-test.zip",
            "size": 121216913,
            "md5": "a26a03ea0b5e88e272437def2555b379"
        },
        {
            "component": "client",
            "platform": "windows_x64",
            "file": "nxwitness-client_update-4.0.0.29069-win64-beta-test.zip",
            "size": 96953959,
            "md5": "aedf0b757dc806a1980858ecfcb805f3"
        }
    ]
})";

} // namespace


namespace nx::vms::client::desktop {

class UpdateVerificationTest: public ClientUpdateTestEnvironment
{
};

TEST_F(UpdateVerificationTest, testAlreadyInstalled)
{
    // This test uses cases from VMS-13236. It also relates to VMS-13811, when we should show
    // 'The latest version' after we have completed update just now.
    nx::update::UpdateContents contents;
    contents.sourceType = nx::update::UpdateSourceType::internetSpecific;
    ASSERT_TRUE(QJson::deserialize<nx::update::Information>(packageRawData, &contents.info));
    ASSERT_FALSE(contents.isEmpty());

    ClientVerificationData clientData;
    VerificationOptions options;

    // Update to 4.0.0.28524
    // client = 4.0.0.28524
    // server = 4.0.0.28525
    // Showing version + 'You have already installed this version' message.
    makeServer(Version("4.0.0.28525"));
    clientData = makeClientData(Version("4.0.0.28524"));

    verifyUpdateContents(contents, getAllServers(), clientData, options);
    EXPECT_EQ(contents.alreadyInstalled, true);
    EXPECT_TRUE(contents.peersWithUpdate.empty());
    // VMS-13236 expects there is noError.
    // Recent changes to VMS-14494 make this return incompatibleVersion.
    EXPECT_EQ(contents.error, nx::update::InformationError::incompatibleVersion);
    {
        const auto report = MultiServerUpdatesWidget::calculateUpdateVersionReport(
            contents, clientData.clientId);
        EXPECT_FALSE(report.hasLatestVersion);
    }
    contents.resetVerification();
    removeAllServers();

    // Update to 4.0.0.28524
    // client = 4.0.0.28525
    // server = 4.0.0.28524
    // Showing page 'This version is already installed'.
    makeServer(Version("4.0.0.28524"));
    clientData = makeClientData(Version("4.0.0.28525"));
    clientData.installedVersions.insert(Version("4.0.0.28524"));
    verifyUpdateContents(contents, getAllServers(), clientData, options);
    EXPECT_EQ(contents.error, nx::update::InformationError::noError);
    {
        const auto report = MultiServerUpdatesWidget::calculateUpdateVersionReport(
            contents, clientData.clientId);
        EXPECT_TRUE(report.hasLatestVersion);
    }
    contents.resetVerification();
    removeAllServers();

    // Update to 4.0.0.28524
    // client = 4.0.0.28524
    // server = 4.0.0.28524
    // server = 4.0.0.28523 offline
    // Showing page 'This version is already installed', even when we have offline server with
    // lower version.
    makeServer(Version("4.0.0.28524"));
    makeServer(Version("4.0.0.28523"), /*online=*/false);
    clientData = makeClientData(Version("4.0.0.28524"));
    verifyUpdateContents(contents, getAllServers(), clientData, options);
    EXPECT_EQ(contents.alreadyInstalled, true);
    {
        const auto report = MultiServerUpdatesWidget::calculateUpdateVersionReport(contents, clientData.clientId);
        EXPECT_TRUE(report.hasLatestVersion);
    }
    removeAllServers();

    // According to VMS-15430, VMS-15250, we should show 'Latest version installed'
    // Update to 4.0.0.28524
    // client = 4.0.0.28526
    // server = 4.0.0.28526
    // Showing page 'This version is already installed'
    contents.sourceType = nx::update::UpdateSourceType::internet;
    makeServer(Version("4.0.0.28524"));
    makeServer(Version("4.0.0.28523"), /*online=*/false);
    clientData = makeClientData(Version("4.0.0.28524"));
    verifyUpdateContents(contents, getAllServers(), clientData, options);
    EXPECT_EQ(contents.alreadyInstalled, true);
    {
        const auto report = MultiServerUpdatesWidget::calculateUpdateVersionReport(contents, clientData.clientId);
        EXPECT_TRUE(report.hasLatestVersion);
    }
    removeAllServers();
}

TEST_F(ClientUpdateTestEnvironment, testForkedVersion)
{
    /**
     * According to VMS-7768, verification should ignore servers newer than target update version
     * There can be a situation when system can contain several servers with lower version and
     * several servers with higher version. We should not prevent servers with lower version to
     * be updated.
     */
    nx::update::UpdateContents contents;
    contents.sourceType = nx::update::UpdateSourceType::internetSpecific;
    ASSERT_TRUE(QJson::deserialize<nx::update::Information>(packageRawData, &contents.info));
    ASSERT_FALSE(contents.isEmpty());
    EXPECT_EQ(contents.getVersion(), Version("4.0.0.28524"));
    VerificationOptions options;

    // Update to 4.0.0.28524
    // client = 4.0.0.28523
    // server1 = 4.0.0.28523
    // server2 = 4.0.0.28525
    ClientVerificationData clientData = makeClientData(Version("4.0.0.28523"));
    makeServer(Version("4.0.0.28523"));
    makeServer(Version("4.0.0.28525"));

    verifyUpdateContents(contents, getAllServers(), clientData, options);
    EXPECT_EQ(contents.error, nx::update::InformationError::noError);
    removeAllServers();
    contents.resetVerification();

    // Both servers are newer, but the client is older.
    // It should be fine to start update only for a client.
    makeServer(Version("4.0.0.28525"));
    makeServer(Version("4.0.0.28525"));
    verifyUpdateContents(contents, getAllServers(), clientData, options);
    EXPECT_EQ(contents.error, nx::update::InformationError::noError);
    removeAllServers();
    contents.resetVerification();

    // Both servers and a client are newer.
    // According to VMS-14814, we should show 'downgrade is not possible'
    clientData = makeClientData(Version("4.0.0.28525"));
    // Update to 4.0.0.28524
    makeServer(Version("4.0.0.28525"));
    makeServer(Version("4.0.0.28525"));
    verifyUpdateContents(contents, getAllServers(), clientData, options);
    EXPECT_EQ(contents.error, nx::update::InformationError::incompatibleVersion);
    EXPECT_TRUE(contents.alreadyInstalled);
    removeAllServers();
}

TEST_F(UpdateVerificationTest, packagesForSystemSupportTest)
{
    nx::update::UpdateContents contents;
    contents.sourceType = nx::update::UpdateSourceType::internetSpecific;
    ASSERT_TRUE(QJson::deserialize<nx::update::Information>(packagesForSystemSupportTest,
        &contents.info));
    ASSERT_FALSE(contents.isEmpty());
    VerificationOptions options;

    // Update to 4.0.0.29069
    // client = 4.0.0.29067
    // server = 4.0.0.29067
    auto server = makeServer(Version("4.0.0.29067"));
    server->setOsInfo(os::ubuntu16);
    ClientVerificationData clientData = makeClientData(Version("4.0.0.29067"));
    clientData.osInfo = os::windows;
    auto servers = getAllServers();
    verifyUpdateContents(contents, servers, clientData, options);
    EXPECT_EQ(contents.error, nx::update::InformationError::missingPackageError);
    // There should be one unsupported system.
    EXPECT_EQ(contents.unsuportedSystemsReport.size(), 1);
    // And zero servers with missing update file.
    EXPECT_EQ(contents.missingUpdate.size(), 0);

    auto report = MultiServerUpdatesWidget::calculateUpdateVersionReport(
        contents, clientData.clientId);
    EXPECT_EQ(report.statusHighlight, MultiServerUpdatesWidget::VersionReport::HighlightMode::red);
    removeAllServers();
}

TEST_F(UpdateVerificationTest, regularUpdateWithoutErrors)
{
    nx::update::UpdateContents contents;
    contents.sourceType = nx::update::UpdateSourceType::internetSpecific;
    ASSERT_TRUE(QJson::deserialize<nx::update::Information>(packageRawData, &contents.info));
    ASSERT_FALSE(contents.isEmpty());
    EXPECT_EQ(contents.getVersion(), Version("4.0.0.28524"));
    VerificationOptions options;

    // Update to 4.0.0.28524
    // client = 4.0.0.28523
    // server1 = 4.0.0.28523
    ClientVerificationData clientData = makeClientData(Version("4.0.0.28523"));
    makeServer(Version("4.0.0.28523"));

    // Expecting that both client and server should be updatedm without any verification errors.
    verifyUpdateContents(contents, getAllServers(), clientData, options);
    EXPECT_EQ(contents.error, nx::update::InformationError::noError);
    EXPECT_TRUE(contents.needClientUpdate);
    EXPECT_TRUE(contents.clientPackage.isValid());
    EXPECT_EQ(contents.peersWithUpdate.size(), 2);
}


TEST_F(UpdateVerificationTest, bestVariantVersionSelection)
{
    const auto& info = QJson::deserialized<nx::update::Information>(
        R"({
            "packages": [
                {
                    "component": "server",
                    "platform": "linux_x64",
                    "variants": [{ "name": "ubuntu", "minimumVersion": "14.04" }],
                    "file": "ubuntu_14.04.zip"
                },
                {
                    "component": "server",
                    "platform": "linux_x64",
                    "variants": [{ "name": "ubuntu", "minimumVersion": "16.04" }],
                    "file": "ubuntu_16.04.zip"
                },
                {
                    "component": "server",
                    "platform": "linux_x64",
                    "variants": [{ "name": "ubuntu", "minimumVersion": "18.04" }],
                    "file": "ubuntu_18.04.zip"
                }
            ]
        })");

    auto [result, package] = nx::update::findPackageForVariant(info, false, os::ubuntu16);
    ASSERT_EQ(result, nx::update::FindPackageResult::ok);
    ASSERT_EQ(package->file, "ubuntu_16.04.zip");
}

TEST_F(UpdateVerificationTest, variantVersionBoundaries)
{
    const auto& info = QJson::deserialized<nx::update::Information>(
        R"({
            "packages": [
                {
                    "component": "server",
                    "platform": "linux_x64",
                    "variants": [
                        {
                            "name": "ubuntu",
                            "minimumVersion": "14.04",
                            "maximumVersion": "16.04"
                        }],
                    "file": "ubuntu.zip"
                }
            ]
        })");

    auto [result, package] = nx::update::findPackageForVariant(info, false, os::ubuntu18);
    ASSERT_EQ(result, nx::update::FindPackageResult::osVersionNotSupported);
}

TEST_F(UpdateVerificationTest, emptyOsVersionAndVersionedPackage)
{
    const auto& info = QJson::deserialized<nx::update::Information>(
        R"({
            "packages": [
                {
                    "component": "server",
                    "platform": "linux_x64",
                    "variants": [{ "name": "ubuntu", "minimumVersion": "14.04" }],
                    "file": "ubuntu.zip"
                }
            ]
        })");

    auto [result, package] = nx::update::findPackageForVariant(info, false, os::ubuntu);
    ASSERT_EQ(result, nx::update::FindPackageResult::osVersionNotSupported);
}

TEST_F(UpdateVerificationTest, preferPackageForCertainVariant)
{
    const auto& info = QJson::deserialized<nx::update::Information>(
        R"({
            "packages": [
                {
                    "component": "server",
                    "platform": "linux_x64",
                    "file": "universal.zip"
                },
                {
                    "component": "server",
                    "platform": "linux_x64",
                    "variants": [{ "name": "ubuntu" }],
                    "file": "ubuntu.zip"
                }
            ]
        })");

    auto [result, package] = nx::update::findPackageForVariant(info, false, os::ubuntu);
    ASSERT_EQ(result, nx::update::FindPackageResult::ok);
    ASSERT_EQ(package->file, "ubuntu.zip");
}

TEST_F(UpdateVerificationTest, preferVersionedVariant)
{
    const auto& info = QJson::deserialized<nx::update::Information>(
        R"({
            "packages": [
                {
                    "component": "server",
                    "platform": "linux_x64",
                    "variants": [{ "name": "ubuntu" }],
                    "file": "ubuntu.zip"
                },
                {
                    "component": "server",
                    "platform": "linux_x64",
                    "variants": [{ "name": "ubuntu", "minimumVersion": "16.04" }],
                    "file": "ubuntu_16.04.zip"
                }
            ]
        })");

    auto [result, package] = nx::update::findPackageForVariant(info, false, os::ubuntu16);
    ASSERT_EQ(result, nx::update::FindPackageResult::ok);
    ASSERT_EQ(package->file, "ubuntu_16.04.zip");
}

TEST_F(UpdateVerificationTest, aSyntheticTest)
{
    const auto& info = QJson::deserialized<nx::update::Information>(
        R"({
            "packages": [
                {
                    "component": "server",
                    "platform": "linux_x64",
                    "file": "universal.zip"
                },
                {
                    "component": "server",
                    "platform": "linux_x64",
                    "variants": [{ "name": "ubuntu" }],
                    "file": "ubuntu.zip"
                },
                {
                    "component": "server",
                    "platform": "linux_x64",
                    "variants": [{ "name": "ubuntu", "minimumVersion": "18.04" }],
                    "file": "ubuntu_18.04.zip"
                }
            ]
        })");

    auto [result, package] = nx::update::findPackageForVariant(info, false, os::ubuntu14);
    ASSERT_EQ(result, nx::update::FindPackageResult::ok);
    ASSERT_EQ(package->file, "ubuntu.zip");
}

} // namespace nx::vms::client::desktop
