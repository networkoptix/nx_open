#include <gtest/gtest.h>

// TODO: This more like mediaserver_core functionality, so that it has to be in mediaserver_core_ut.

#include <core/resource/resource_type.h>
#include <nx_ec/ec_api.h>
#include <nx/network/app_info.h>
#include <nx/network/cloud/cloud_connect_controller.h>
#include <nx/network/socket_global.h>
#include <nx/utils/log/log.h>
#include <nx/utils/test_support/module_instance_launcher.h>
#include <nx/utils/test_support/test_options.h>
#include <nx/vms/discovery/manager.h>
#include <test_support/appserver2_process.h>

static const std::chrono::milliseconds kDiscoveryTimeouts(100);
static const std::chrono::seconds kWaitForDiscoveryTimeout(10);
size_t kInitialServerCount = 3;
size_t kAdditionalServerCount = 2;
nx::network::RetryPolicy kReconnectPolicy(
    nx::network::RetryPolicy::kInfiniteRetries, kDiscoveryTimeouts, 1, kDiscoveryTimeouts);

class DiscoveryTest: public testing::Test
{
protected:
    typedef nx::utils::test::ModuleLauncher<::ec2::Appserver2Process> MediaServer;

    void addServer()
    {
        static int instanceCounter = 0;
        const auto tmpDir = nx::utils::TestOptions::temporaryDirectoryPath() +
            lit("/ec2_discovery_ut.data") + QString::number(instanceCounter++);
        QDir(tmpDir).removeRecursively();

        auto server = std::make_unique<MediaServer>();
        server->addArg(lit("--dbFile=%1").arg(tmpDir).toStdString().c_str());
        server->addArg(lit("--disableAuth").toStdString().c_str());
        ASSERT_TRUE(server->startAndWaitUntilStarted());

        const auto module = server->moduleInstance().get();
        module->commonModule()->updateRunningInstanceGuid();
        ASSERT_TRUE(module);
        initServerData(module);
        NX_ALWAYS(this, lm("Server %1 started at %2").args(
            module->commonModule()->moduleGUID(), server->moduleInstance()->endpoint()));

        const auto discoveryManager = module->commonModule()->moduleDiscoveryManager();
        discoveryManager->setReconnectPolicy(kReconnectPolicy);
        discoveryManager->setMulticastInterval(kDiscoveryTimeouts);
        discoveryManager->start();
        m_servers.emplace(module->commonModule()->moduleGUID(), std::move(server));
    }

    void initServerData(::ec2::Appserver2Process* module)
    {
        nx::utils::SoftwareVersion version(1, 2, 3, 123);
        const auto connection = module->ecConnection();
        ASSERT_NE(nullptr, connection);

        const auto resourceManager = connection->getResourceManager(Qn::kSystemAccess);
        ASSERT_NE(nullptr, resourceManager);

        QList<QnResourceTypePtr> resourceTypeList;
        ASSERT_EQ(ec2::ErrorCode::ok, resourceManager->getResourceTypesSync(&resourceTypeList));
        qnResTypePool->replaceResourceTypeList(resourceTypeList);

        auto resTypePtr = qnResTypePool->getResourceTypeByName("Server");
        ASSERT_TRUE(!resTypePtr.isNull());

        nx::vms::api::MediaServerData serverData;
        serverData.typeId = resTypePtr->getId();
        serverData.id = module->commonModule()->moduleGUID();
        serverData.url = lit("https://127.0.0.1:7001");
        serverData.authKey = QnUuid::createUuid().toString();
        serverData.name = lm("server %1").arg(serverData.id);
        ASSERT_TRUE(!resTypePtr.isNull());
        serverData.typeId = resTypePtr->getId();
        serverData.networkAddresses = module->endpoint().toString();
        serverData.version = version.toString();

        nx::vms::api::ModuleInformation information;
        information.type = nx::vms::api::ModuleInformation::nxMediaServerId();
        information.id = serverData.id;
        information.name = serverData.name;
        information.version = version;
        information.runtimeId = module->commonModule()->runningInstanceGUID();
        information.realm = nx::network::AppInfo::realm();
        information.cloudHost = nx::network::SocketGlobals::cloud().cloudHost();
        module->commonModule()->setModuleInformation(information);

        auto serverManager = connection->getMediaServerManager(Qn::kSystemAccess);
        ASSERT_EQ(ec2::ErrorCode::ok, serverManager->saveSync(serverData));
    }

    void checkVisibility()
    {
        size_t totalDiscoveryLinks = 0;
        #if defined(Q_OS_MAC)
            // Can join different UDT sockets to the same multicast group on OSX. So at least one
            // should be able to see everyone else.
            const size_t expectedDiscoveryLinks = m_servers.size() - 1;
        #else
            // Everyone should be able to see everyone else.
            const size_t expectedDiscoveryLinks = m_servers.size() * (m_servers.size() - 1);
        #endif

        const auto startTime = std::chrono::steady_clock::now();
        while (std::chrono::steady_clock::now() - startTime < kWaitForDiscoveryTimeout)
        {
            totalDiscoveryLinks = 0;
            for (const auto& server: m_servers)
            {
                totalDiscoveryLinks += checkVisibilityFor(
                    server.second->moduleInstance()->commonModule());
            }

            if (expectedDiscoveryLinks == totalDiscoveryLinks)
                return;

            std::this_thread::sleep_for(kDiscoveryTimeouts);
        }

        EXPECT_EQ(expectedDiscoveryLinks, totalDiscoveryLinks);
    }

    size_t checkVisibilityFor(const QnCommonModule* searcher)
    {
        size_t totalDiscoveryLinks = 0;
        const auto discoveryManager = searcher->moduleDiscoveryManager();
        for (const auto& server: m_servers)
        {
            if (searcher->moduleGUID() == server.first)
                continue;

            if (const auto module = discoveryManager->getModule(server.first))
            {
                ++totalDiscoveryLinks;
                NX_ALWAYS(this, lm("Module %1 discovered %2 with endpoint %3").args(
                    searcher->moduleGUID(), module->id, module->endpoint));

                EXPECT_EQ(module->id.toString(), server.first.toString());
                EXPECT_EQ(module->endpoint.port, server.second->moduleInstance()->endpoint().port);
            }
            else
            {
                NX_WARNING(this, lm("Module %1 failed to discover %2").args(
                    searcher->moduleGUID(), server.first));
            }
        }

        return totalDiscoveryLinks;
    }

private:
    std::map<QnUuid, std::unique_ptr<MediaServer>> m_servers;
};

TEST_F(DiscoveryTest, main)
{
    for (size_t i = 0; i < kInitialServerCount; ++i)
        addServer();
    checkVisibility();

    for (size_t i = 0; i < kAdditionalServerCount; ++i)
        addServer();
    checkVisibility();
}
