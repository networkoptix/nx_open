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

#include "nx/vms/client/desktop/system_update/update_contents.h"
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
      "arch": "x64",
      "platform": "windows",
      "variant": "winxp",
      "file": "nxwitness-server_update-4.0.0.28524-win64-beta-prod.zip",
      "size": 89479542,
      "md5": "88d9abcaae8ddf076fa1fe9cc829016a"
    },
    {
      "component": "client",
      "arch": "x64",
      "platform": "windows",
      "variant": "winxp",
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
      "arch": "x64",
      "platform": "linux",
      "variant": "ubuntu",
      "variantVersion": "18.04",
      "file": "nxwitness-server_update-4.0.0.29069-linux64-beta-test.zip",
      "size": 121216913,
      "md5": "a26a03ea0b5e88e272437def2555b379"
    },
    {
      "component": "client",
      "arch": "x64",
      "platform": "windows",
      "variant": "winxp",
      "file": "nxwitness-client_update-4.0.0.29069-win64-beta-test.zip",
      "size": 96953959,
      "md5": "aedf0b757dc806a1980858ecfcb805f3"
    }
  ]
})";

struct OS
{
    static nx::vms::api::SystemInformation ubuntu14(const QString& arch="x64")
    {
        nx::vms::api::SystemInformation info;
        info.arch = arch;
        info.platform = "linux";
        info.modification = "ubuntu";
        info.version = "14.04";
        return info;
    }

    static nx::vms::api::SystemInformation ubuntu16(const QString& arch="x64")
    {
        nx::vms::api::SystemInformation info;
        info.arch = arch;
        info.platform = "linux";
        info.modification = "ubuntu";
        info.version = "16.04";
        return info;
    }

    static nx::vms::api::SystemInformation ubuntu18(const QString& arch="x64")
    {
        nx::vms::api::SystemInformation info;
        info.arch = arch;
        info.platform = "linux";
        info.modification = "ubuntu";
        info.version = "18.04";
        return info;
    }

    static nx::vms::api::SystemInformation windows()
    {
        nx::vms::api::SystemInformation info;
        info.arch = "x64";
        info.platform = "windows";
        info.modification = "winxp";
        info.version = "";
        return info;
    }
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

    api::SystemInformation makeSystemInfo()
    {
        api::SystemInformation info;
        info.arch = "x64";
        info.platform = "windows";
        info.modification = "winxp";
        return info;
    }

    ClientVerificationData makeClientData(nx::utils::SoftwareVersion version)
    {
        ClientVerificationData data;
        data.systemInfo = makeSystemInfo();
        data.currentVersion = version;
        data.clientId = QnUuid("cccccccc-cccc-cccc-cccc-cccccccccccc");
        return data;
    }

    QnMediaServerResourcePtr makeServer(nx::utils::SoftwareVersion version, bool online = true)
    {
        QnMediaServerResourcePtr server(new QnMediaServerResource(commonModule()));
        server->setVersion(version);
        server->setId(QnUuid::createUuid());
        server->setSystemInfo(makeSystemInfo());
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
    EXPECT_EQ(contents.error, nx::update::InformationError::incompatibleVersion);
    {
        const auto report = MultiServerUpdatesWidget::calculateUpdateVersionReport(
            contents, clientData.clientId);
        EXPECT_TRUE(report.hasLatestVersion);
    }
    removeAllServers();

    // Update to 4.0.0.28524
    // client = 4.0.0.28525
    // server = 4.0.0.28524
    // Showing page 'This version is already installed'.
    makeServer(nx::utils::SoftwareVersion("4.0.0.28524"));
    clientData = makeClientData(nx::utils::SoftwareVersion("4.0.0.28525"));
    verifyUpdateContents(nullptr, contents, getAllServers(), clientData);
    EXPECT_EQ(contents.error, nx::update::InformationError::incompatibleVersion);
    {
        const auto report = MultiServerUpdatesWidget::calculateUpdateVersionReport(
            contents, clientData.clientId);
        EXPECT_TRUE(report.hasLatestVersion);
    }
    removeAllServers();

    // Update to 4.0.0.28524
    // client = 4.0.0.28524
    // server = 4.0.0.28524
    // server = 4.0.0.28523 offline
    // Showing page 'This version is already installed', even when we have offline server with
    // lower version.
    makeServer(nx::utils::SoftwareVersion("4.0.0.28524"));
    makeServer(nx::utils::SoftwareVersion("4.0.0.28523"), false);
    clientData = makeClientData(nx::utils::SoftwareVersion("4.0.0.28524"));
    verifyUpdateContents(nullptr, contents, getAllServers(), clientData);
    EXPECT_EQ(contents.alreadyInstalled, true);
    {
        const auto report = MultiServerUpdatesWidget::calculateUpdateVersionReport(contents, clientData.clientId);
        EXPECT_TRUE(report.hasLatestVersion);
    }
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
    // Showing page 'This version is already installed'.
    auto server = makeServer(nx::utils::SoftwareVersion("4.0.0.29067"));
    server->setSystemInfo(OS::ubuntu16());
    ClientVerificationData clientData = makeClientData(nx::utils::SoftwareVersion("4.0.0.29067"));
    clientData.systemInfo = OS::windows();
    auto servers = getAllServers();
    verifyUpdateContents(nullptr, contents, servers, clientData);
    EXPECT_EQ(contents.error, nx::update::InformationError::missingPackageError);
    // There should be one unsupported system.
    EXPECT_EQ(contents.unsuportedSystemsReport.size(), 1);
    // And zero servers with missing update file.
    EXPECT_EQ(contents.missingUpdate.size(), 0);

    auto report = MultiServerUpdatesWidget::calculateUpdateVersionReport(
        contents, clientData.clientId);
    EXPECT_TRUE(report.statusHighlight == MultiServerUpdatesWidget::VersionReport::HighlightMode::red);
}

} // namespace nx::vms::client::desktop
