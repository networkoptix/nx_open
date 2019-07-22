#include <gtest/gtest.h>

#include <common/common_module.h>
#include <common/static_common_module.h>

#include <client/client_runtime_settings.h>
#include <client/client_startup_parameters.h>
#include <client_core/client_core_module.h>

#include <core/resource_management/resource_pool.h>
#include <core/resource_management/resource_runtime_data.h>
#include <core/resource/media_server_resource.h>

#include <ui/workbench/workbench_access_controller.h>
#include <ui/workbench/workbench_context.h>

#include "nx/vms/client/desktop/system_update/update_verification.h"
#include "nx/vms/client/desktop/system_update/multi_server_updates_widget.h"

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

namespace os
{
    const nx::utils::OsInfo ubuntu("linux_x64", "ubuntu");
    const nx::utils::OsInfo ubuntu14("linux_x64", "ubuntu", "14.04");
    const nx::utils::OsInfo ubuntu16("linux_x64", "ubuntu", "16.04");
    const nx::utils::OsInfo ubuntu18("linux_x64", "ubuntu", "18.04");
    const nx::utils::OsInfo windows("windows_x64");
};

} // namespace

namespace nx::vms::client::desktop {

class UpdateVerificationTest: public testing::Test
{
public:
    // virtual void SetUp() will be called before each test is run.
    virtual void SetUp()
    {
        m_runtime.reset(new QnClientRuntimeSettings(QnStartupParameters()));
        m_staticCommon.reset(new QnStaticCommonModule());
        m_module.reset(new QnClientCoreModule());
        m_resourceRuntime.reset(new QnResourceRuntimeDataManager(m_module->commonModule()));
        m_accessController.reset(new QnWorkbenchAccessController(m_module->commonModule()));
    }

    // virtual void TearDown() will be called after each test is run.
    virtual void TearDown()
    {
        m_currentUser.clear();
        m_accessController.clear();
        m_resourceRuntime.clear();
        m_module.clear();
        m_staticCommon.reset();
        m_runtime.clear();
    }

    ClientVerificationData makeClientData(nx::utils::SoftwareVersion version)
    {
        ClientVerificationData data;
        data.osInfo = os::windows;
        data.currentVersion = version;
        data.clientId = QnUuid("cccccccc-cccc-cccc-cccc-cccccccccccc");
        return data;
    }

    QnMediaServerResourcePtr makeServer(nx::utils::SoftwareVersion version, bool online = true)
    {
        QnMediaServerResourcePtr server(new QnMediaServerResource(commonModule()));
        server->setVersion(version);
        server->setId(QnUuid::createUuid());
        server->setOsInfo(os::windows);
        server->setStatus(online ? Qn::ResourceStatus::Online : Qn::ResourceStatus::Offline);

        resourcePool()->addResource(server);
        return server;
    }

    std::map<QnUuid, QnMediaServerResourcePtr> getAllServers()
    {
        std::map<QnUuid, QnMediaServerResourcePtr> result;
        for (auto server: resourcePool()->getAllServers(Qn::ResourceStatus::AnyStatus))
            result[server->getId()] = server;
        return result;
    }

    void removeAllServers()
    {
        auto servers = resourcePool()->getAllServers(Qn::ResourceStatus::AnyStatus);
        resourcePool()->removeResources(servers);
    }

    QnCommonModule* commonModule() const { return m_module->commonModule(); }
    QnResourcePool* resourcePool() const { return commonModule()->resourcePool(); }

    // Declares the variables your tests want to use.
    QScopedPointer<QnStaticCommonModule> m_staticCommon;
    QSharedPointer<QnClientCoreModule> m_module;
    QSharedPointer<QnWorkbenchAccessController> m_accessController;
    QSharedPointer<QnClientRuntimeSettings> m_runtime;
    QSharedPointer<QnResourceRuntimeDataManager> m_resourceRuntime;
    QnUserResourcePtr m_currentUser;
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

    // Update to 4.0.0.28524
    // client = 4.0.0.28524
    // server = 4.0.0.28525
    // Showing version + 'You have already installed this version' message.
    makeServer(nx::utils::SoftwareVersion("4.0.0.28525"));
    clientData = makeClientData(nx::utils::SoftwareVersion("4.0.0.28524"));

    verifyUpdateContents(nullptr, contents, getAllServers(), clientData);
    EXPECT_EQ(contents.alreadyInstalled, true);
    EXPECT_TRUE(contents.peersWithUpdate.empty());
    // VMS-13236 expects there is noError.
    // Recent changes to VMS-14494 make this return incompatibleVersion.
    EXPECT_EQ(contents.error, nx::update::InformationError::incompatibleVersion);
    {
        const auto report = MultiServerUpdatesWidget::calculateUpdateVersionReport(
            contents, clientData.clientId);
        EXPECT_TRUE(report.hasLatestVersion);
    }
    contents.resetVerification();
    removeAllServers();

    // Update to 4.0.0.28524
    // client = 4.0.0.28525
    // server = 4.0.0.28524
    // Showing page 'This version is already installed'.
    makeServer(nx::utils::SoftwareVersion("4.0.0.28524"));
    clientData = makeClientData(nx::utils::SoftwareVersion("4.0.0.28525"));
    clientData.installedVersions.insert(nx::utils::SoftwareVersion("4.0.0.28524"));
    verifyUpdateContents(nullptr, contents, getAllServers(), clientData);
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
    makeServer(nx::utils::SoftwareVersion("4.0.0.28524"));
    makeServer(nx::utils::SoftwareVersion("4.0.0.28523"), /*online=*/false);
    clientData = makeClientData(nx::utils::SoftwareVersion("4.0.0.28524"));
    verifyUpdateContents(nullptr, contents, getAllServers(), clientData);
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
    nx::update::UpdateContents contents;
    contents.sourceType = nx::update::UpdateSourceType::internetSpecific;
    ASSERT_TRUE(QJson::deserialize<nx::update::Information>(packageRawData, &contents.info));
    ASSERT_FALSE(contents.isEmpty());
    EXPECT_EQ(contents.getVersion(), nx::utils::SoftwareVersion("4.0.0.28524"));

    // Update to 4.0.0.28524
    // client = 4.0.0.28523
    // server1 = 4.0.0.28523
    // server2 = 4.0.0.28525
    ClientVerificationData clientData = makeClientData(nx::utils::SoftwareVersion("4.0.0.28523"));
    makeServer(nx::utils::SoftwareVersion("4.0.0.28523"));
    makeServer(nx::utils::SoftwareVersion("4.0.0.28525"));

    verifyUpdateContents(nullptr, contents, getAllServers(), clientData);
    EXPECT_EQ(contents.error, nx::update::InformationError::noError);
    removeAllServers();
    contents.resetVerification();

    // Both servers are newer, but the client is older.
    // It should be fine to start update only for a client.
    makeServer(nx::utils::SoftwareVersion("4.0.0.28525"));
    makeServer(nx::utils::SoftwareVersion("4.0.0.28525"));
    verifyUpdateContents(nullptr, contents, getAllServers(), clientData);
    EXPECT_EQ(contents.error, nx::update::InformationError::noError);
    removeAllServers();
    contents.resetVerification();

    // Both servers and a client are newer.
    // According to VMS-14814, we should show 'downgrade is not possible'
    clientData = makeClientData(nx::utils::SoftwareVersion("4.0.0.28525"));
    makeServer(nx::utils::SoftwareVersion("4.0.0.28525"));
    makeServer(nx::utils::SoftwareVersion("4.0.0.28525"));
    verifyUpdateContents(nullptr, contents, getAllServers(), clientData);
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

    // Update to 4.0.0.29069
    // client = 4.0.0.29067
    // server = 4.0.0.29067
    auto server = makeServer(nx::utils::SoftwareVersion("4.0.0.29067"));
    server->setOsInfo(os::ubuntu16);
    ClientVerificationData clientData = makeClientData(nx::utils::SoftwareVersion("4.0.0.29067"));
    clientData.osInfo = os::windows;
    auto servers = getAllServers();
    verifyUpdateContents(nullptr, contents, servers, clientData);
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
