// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include <common/common_module.h>
#include <common/static_common_module.h>
#include <nx/vms/client/desktop/system_update/multi_server_updates_widget.h>
#include <nx/vms/common/system_context.h>
#include <nx/vms/common/system_settings.h>
#include <nx/vms/client/desktop/system_context.h>

#include "client_update_test_environment.h"

namespace {

const QString packageRawData = R"(
{
    "version": "4.0.0.28524",
    "cloudHost": "testvms.cloud",
    "releaseNotesUrl": "http://testvms.local/all-release-notes",
    "description": "",
    "eulaLink": "eula-1.html",
    "eulaVersion": 1,
    "packages": [
        {
            "component": "server",
            "platform": "windows_x64",
            "file": "vms-server_update-4.0.0.28524-win64-beta-prod.zip",
            "size": 89479542,
            "md5": "88d9abcaae8ddf076fa1fe9cc829016a"
        },
        {
            "component": "client",
            "platform": "windows_x64",
            "file": "vms-client_update-4.0.0.28524-win64-beta-prod.zip",
            "size": 99609610,
            "md5": "bd823350defd778c9990b908d90f92aa"
        }
    ]
})";

const QString packagesForSystemSupportTest = R"(
{
    "version": "4.0.0.29069",
    "cloudHost": "testvms.cloud",
    "releaseNotesUrl": "http://testvms.local/all-release-notes",
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
            "file": "vms-server_update-4.0.0.29069-linux64-beta-test.zip",
            "size": 121216913,
            "md5": "a26a03ea0b5e88e272437def2555b379"
        },
        {
            "component": "client",
            "platform": "windows_x64",
            "file": "vms-client_update-4.0.0.29069-win64-beta-test.zip",
            "size": 96953959,
            "md5": "aedf0b757dc806a1980858ecfcb805f3"
        }
    ]
})";

} // namespace


namespace nx::vms::client::desktop::test {

using namespace system_update::test;

class UpdateVerificationTest: public ClientUpdateTestEnvironment
{
};

using FindPackageResult = update::PublicationInfo::FindPackageResult;

TEST_F(UpdateVerificationTest, trivialUpdateCheck)
{
    UpdateContents contents;
    contents.sourceType = UpdateSourceType::internet;
    ASSERT_TRUE(QJson::deserialize<common::update::Information>(packageRawData, &contents.info));
    ASSERT_FALSE(contents.isEmpty());

    const Version kCurrentVersion("4.0.0.28000");
    makeServer(kCurrentVersion);
    const auto clientData = makeClientData(kCurrentVersion);

    verifyUpdateContents(contents, getAllServers(), clientData, VerificationOptions());
    EXPECT_EQ(contents.error, common::update::InformationError::noError);
    EXPECT_TRUE(contents.needClientUpdate);
    EXPECT_EQ(contents.peersWithUpdate.size(), 2); //< Client + Server.
    EXPECT_FALSE(contents.alreadyInstalled);

    const auto report = MultiServerUpdatesWidget::calculateUpdateVersionReport(
        contents, clientData.clientId);
    EXPECT_FALSE(report.hasLatestVersion);
}

TEST_F(UpdateVerificationTest, testAlreadyInstalled)
{
    // This test uses cases from VMS-13236. It also relates to VMS-13811, when we should show
    // 'The latest version' after we have completed update just now.
    UpdateContents contents;
    contents.sourceType = UpdateSourceType::internetSpecific;
    ASSERT_TRUE(QJson::deserialize<common::update::Information>(packageRawData, &contents.info));
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
    EXPECT_EQ(contents.error, common::update::InformationError::incompatibleVersion);
    {
        const auto report = MultiServerUpdatesWidget::calculateUpdateVersionReport(
            contents, clientData.clientId);
        EXPECT_FALSE(report.hasLatestVersion);
    }
    contents.resetVerification();
    removeAllServers();

    // Update to 4.0.0.28524
    // client = 4.0.0.28525, installed = {4.0.0.28524}
    // server = 4.0.0.28524
    // Showing page 'This version is already installed'.
    makeServer(Version("4.0.0.28524"));
    clientData = makeClientData(Version("4.0.0.28525"));
    clientData.installedVersions.insert(Version("4.0.0.28524"));
    verifyUpdateContents(contents, getAllServers(), clientData, options);
    EXPECT_FALSE(contents.needClientUpdate);
    EXPECT_EQ(contents.error, common::update::InformationError::noError);
    {
        const auto report = MultiServerUpdatesWidget::calculateUpdateVersionReport(
            contents, clientData.clientId);
        EXPECT_TRUE(report.hasLatestVersion);
    }
    contents.resetVerification();
    removeAllServers();

    // According to VMS-15430, VMS-15250, we should show 'Latest version installed'
    // Update to 4.0.0.28524
    // client = 4.0.0.28526
    // server = 4.0.0.28526
    // Showing page 'This version is already installed'
    contents.sourceType = UpdateSourceType::internet;
    makeServer(Version("4.0.0.28526"));
    makeServer(Version("4.0.0.28526"), /*online=*/false);
    clientData = makeClientData(Version("4.0.0.28524"));
    verifyUpdateContents(contents, getAllServers(), clientData, options);
    EXPECT_FALSE(contents.needClientUpdate);
    EXPECT_EQ(contents.alreadyInstalled, true);
    {
        const auto report = MultiServerUpdatesWidget::calculateUpdateVersionReport(contents, clientData.clientId);
        EXPECT_TRUE(report.hasLatestVersion);
    }
    removeAllServers();
}

TEST_F(UpdateVerificationTest, testForkedVersion)
{
    /**
     * According to VMS-7768, verification should ignore servers newer than target update version
     * There can be a situation when system can contain several servers with lower version and
     * several servers with higher version. We should not prevent servers with lower version to
     * be updated.
     */
    UpdateContents contents;
    contents.sourceType = UpdateSourceType::internetSpecific;
    ASSERT_TRUE(QJson::deserialize<common::update::Information>(packageRawData, &contents.info));
    ASSERT_FALSE(contents.isEmpty());
    EXPECT_EQ(contents.info.version, Version("4.0.0.28524"));
    VerificationOptions options;

    // Update to 4.0.0.28524
    // client = 4.0.0.28523
    // server1 = 4.0.0.28523
    // server2 = 4.0.0.28525
    ClientVerificationData clientData = makeClientData(Version("4.0.0.28523"));
    makeServer(Version("4.0.0.28523"));
    makeServer(Version("4.0.0.28525"));

    verifyUpdateContents(contents, getAllServers(), clientData, options);
    EXPECT_EQ(contents.error, common::update::InformationError::noError);
    removeAllServers();
    contents.resetVerification();

    // Both servers and a client are newer.
    // According to VMS-14814, we should show 'downgrade is not possible' if we have manually
    // picked this version. Note: it should be 'latest version installed' if update source = Latest
    clientData = makeClientData(Version("4.0.0.28525"));
    // Update to 4.0.0.28524
    makeServer(Version("4.0.0.28525"));
    makeServer(Version("4.0.0.28525"));
    verifyUpdateContents(contents, getAllServers(), clientData, options);
    EXPECT_EQ(contents.error, common::update::InformationError::incompatibleVersion);
    EXPECT_TRUE(contents.alreadyInstalled);
    removeAllServers();
    contents.resetVerification();

    // Both servers are newer, but the client is older.
    // It should be fine to start update only for a client.
    clientData = makeClientData(Version("4.0.0.28523"));
    makeServer(Version("4.0.0.28525"));
    makeServer(Version("4.0.0.28525"));
    verifyUpdateContents(contents, getAllServers(), clientData, options);
    EXPECT_TRUE(contents.needClientUpdate);
    EXPECT_EQ(contents.error, common::update::InformationError::noError);
    removeAllServers();
    contents.resetVerification();

    // Both servers have this version, but the client is older.
    // It should be fine to start update only for a client.
    clientData = makeClientData(Version("4.0.0.28523"));
    makeServer(Version("4.0.0.28524"));
    makeServer(Version("4.0.0.28524"));
    verifyUpdateContents(contents, getAllServers(), clientData, options);
    EXPECT_TRUE(contents.needClientUpdate);
    EXPECT_EQ(contents.error, common::update::InformationError::noError);
    removeAllServers();
    contents.resetVerification();

    // Both servers have equal version, but the client is older.
    // It should be fine to start update only for a client.
    contents.sourceType = UpdateSourceType::internetSpecific;
    clientData = makeClientData(Version("4.0.0.28523"));
    makeServer(Version("4.0.0.28524"));
    makeServer(Version("4.0.0.28524"));
    verifyUpdateContents(contents, getAllServers(), clientData, options);
    EXPECT_EQ(contents.error, common::update::InformationError::noError);
    removeAllServers();
    contents.resetVerification();

    // Both servers have equal version, but the client is older.
    // In VMS-14494 one server had no package avaliable, but it had desired version already.
    // We should ignore this server as well;
    contents.sourceType = UpdateSourceType::internetSpecific;
    makeServer(Version("4.0.0.28524"));
    // This server will have no package available.
    makeServer(Version("4.0.0.28524"))->setOsInfo(os::ubuntu16);
    verifyUpdateContents(contents, getAllServers(), clientData, options);
    EXPECT_EQ(contents.error, common::update::InformationError::noError);
    removeAllServers();
    contents.resetVerification();
}

TEST_F(UpdateVerificationTest, testFailedUpdateChechk)
{
    // This test checks how `calculateUpdateVersionReport` reacts to different errors
    using VersionReport = MultiServerUpdatesWidget::VersionReport;
    UpdateContents contents;
    contents.sourceType = UpdateSourceType::internetSpecific;
    contents.info.version = Version("4.0.0.29681");
    auto clientData = makeClientData(Version("4.0.0.29679"));

    // "Unable to check updates on the Internet"
    contents.error = common::update::InformationError::networkError;
    auto report = MultiServerUpdatesWidget::calculateUpdateVersionReport(
        contents, clientData.clientId);
    EXPECT_FALSE(report.hasLatestVersion);
    EXPECT_TRUE(report.versionMode == VersionReport::VersionMode::empty);

    // "Build not found"
    contents.error = common::update::InformationError::httpError;
    report = MultiServerUpdatesWidget::calculateUpdateVersionReport(
        contents, clientData.clientId);
    EXPECT_FALSE(report.hasLatestVersion);
    EXPECT_TRUE(report.versionMode == VersionReport::VersionMode::build);

    // "Unable to check updates on the Internet"
    contents.sourceType = UpdateSourceType::internet;
    report = MultiServerUpdatesWidget::calculateUpdateVersionReport(
        contents, clientData.clientId);
    EXPECT_FALSE(report.hasLatestVersion);
    EXPECT_TRUE(report.versionMode == VersionReport::VersionMode::empty);
}

TEST_F(UpdateVerificationTest, packagesForSystemSupportTest)
{
    UpdateContents contents;
    contents.sourceType = UpdateSourceType::internetSpecific;
    ASSERT_TRUE(QJson::deserialize<common::update::Information>(
        packagesForSystemSupportTest, &contents.info));
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
    EXPECT_EQ(contents.error, common::update::InformationError::missingPackageError);
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
    UpdateContents contents;
    contents.sourceType = UpdateSourceType::internetSpecific;
    ASSERT_TRUE(QJson::deserialize<common::update::Information>(packageRawData, &contents.info));
    ASSERT_FALSE(contents.isEmpty());
    EXPECT_EQ(contents.info.version, Version("4.0.0.28524"));
    VerificationOptions options;

    // Update to 4.0.0.28524
    // client = 4.0.0.28523
    // server1 = 4.0.0.28523
    ClientVerificationData clientData = makeClientData(Version("4.0.0.28523"));
    makeServer(Version("4.0.0.28523"));

    // Expecting that both client and server should be updatedm without any verification errors.
    verifyUpdateContents(contents, getAllServers(), clientData, options);
    EXPECT_EQ(contents.error, common::update::InformationError::noError);
    EXPECT_TRUE(contents.needClientUpdate);
    EXPECT_TRUE(contents.clientPackage);
    EXPECT_EQ(contents.peersWithUpdate.size(), 2);
}

TEST_F(UpdateVerificationTest, serversWithoutInternetShouldGetUpdatesFromClient)
{
    UpdateContents contents;
    contents.sourceType = UpdateSourceType::internet;
    ASSERT_TRUE(QJson::deserialize<common::update::Information>(packageRawData, &contents.info));
    ASSERT_FALSE(contents.isEmpty());

    const Version kCurrentVersion("4.0.0.28000");

    const auto server = makeServer(kCurrentVersion);
    server->removeFlags(Qn::ResourceFlags(nx::vms::api::SF_HasPublicIP));
    ASSERT_FALSE(server->hasInternetAccess());

    const auto clientData = makeClientData(kCurrentVersion);

    verifyUpdateContents(contents, getAllServers(), clientData, VerificationOptions());
    EXPECT_EQ(contents.error, common::update::InformationError::noError);
    EXPECT_FALSE(contents.manualPackages.isEmpty());
}

TEST_F(UpdateVerificationTest, cloudCompatibilityCheck)
{
    UpdateContents contents;
    contents.sourceType = UpdateSourceType::internet;
    ASSERT_TRUE(QJson::deserialize<common::update::Information>(packageRawData, &contents.info));
    ASSERT_FALSE(contents.isEmpty());

    static const QString kCloudSystemId = "cloud_system_id";

    commonModule()->globalSettings()->setCloudSystemId({});

    VerificationOptions options;
    options.systemContext = systemContext();

    const Version kCurrentVersion("4.0.0.28000");
    makeServer(kCurrentVersion);
    const auto clientData = makeClientData(kCurrentVersion);

    commonModule()->globalSettings()->setCloudSystemId({});
    verifyUpdateContents(contents, getAllServers(), clientData, options);
    EXPECT_EQ(contents.error, common::update::InformationError::noError);
    EXPECT_TRUE(contents.cloudIsCompatible);

    commonModule()->globalSettings()->setCloudSystemId(kCloudSystemId);
    verifyUpdateContents(contents, getAllServers(), clientData, options);
    EXPECT_EQ(contents.error, common::update::InformationError::incompatibleCloudHost);
    EXPECT_FALSE(contents.cloudIsCompatible);
}

} // namespace nx::vms::client::desktop::test
