#include <gtest/gtest.h>

// TODO: This more like mediaserver_core functionality, so that it has to be in mediaserver_core_ut.

#include <core/resource/resource_type.h>
#include <nx_ec/ec_api.h>
#include <nx/utils/log/log.h>
#include <nx/utils/test_support/module_instance_launcher.h>
#include <nx/utils/test_support/test_options.h>
#include <nx/vms/discovery/manager.h>
#include <test_support/appserver2_process.h>

static const std::chrono::milliseconds kDiscoveryTimeouts(100);
size_t kInitialServerCount = 3;
size_t kAdditionalServerCount = 2;
nx::network::RetryPolicy kReconnectPolicy(
    nx::network::RetryPolicy::kInfiniteRetries, kDiscoveryTimeouts, 1, kDiscoveryTimeouts);

class DiscoveryTest: public testing::Test
{
protected:
    typedef nx::utils::test::ModuleLauncher<::ec2::Appserver2ProcessPublic> MediaServer;

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

    void initServerData(::ec2::Appserver2ProcessPublic* module)
    {
        QnSoftwareVersion version(1, 2, 3, 123);
        const auto connection = module->ecConnection();
        ASSERT_NE(nullptr, connection);

        const auto resourceManager = connection->getResourceManager(Qn::kSystemAccess);
        ASSERT_NE(nullptr, resourceManager);

        QList<QnResourceTypePtr> resourceTypeList;
        ASSERT_EQ(ec2::ErrorCode::ok, resourceManager->getResourceTypesSync(&resourceTypeList));
        qnResTypePool->replaceResourceTypeList(resourceTypeList);

        auto resTypePtr = qnResTypePool->getResourceTypeByName("Server");
        ASSERT_TRUE(!resTypePtr.isNull());

        ec2::ApiMediaServerData serverData;
        serverData.typeId = resTypePtr->getId();
        serverData.id = module->commonModule()->moduleGUID();
        serverData.authKey = QnUuid::createUuid().toString();
        serverData.name = lm("server %1").arg(serverData.id);
        ASSERT_TRUE(!resTypePtr.isNull());
        serverData.typeId = resTypePtr->getId();
        serverData.networkAddresses = module->endpoint().toString();
        serverData.version = version.toString();

        QnModuleInformation information;
        information.type = QnModuleInformation::nxMediaServerId();
        information.id = serverData.id;
        information.name = serverData.name;
        information.version = version;
        information.runtimeId = module->commonModule()->runningInstanceGUID();
        module->commonModule()->setModuleInformation(information);

        auto serverManager = connection->getMediaServerManager(Qn::kSystemAccess);
        ASSERT_EQ(ec2::ErrorCode::ok, serverManager->saveSync(serverData));
    }

    void checkVisibility()
    {
        // Wait enough time for servers to discover each other.
        std::this_thread::sleep_for(kDiscoveryTimeouts * m_servers.size() * m_servers.size());

        size_t totalDiscoveryLinks = 0;
        for (const auto& server: m_servers)
        {
            const auto discoveryManager = server.second->moduleInstance()->commonModule()
                ->moduleDiscoveryManager();

            for (const auto& otherServer: m_servers)
            {
                if (server.first == otherServer.first)
                    continue;

                if (const auto module = discoveryManager->getModule(otherServer.first))
                {
                    ++totalDiscoveryLinks;
                    NX_ALWAYS(this, lm("Module %1 discovered %2 with endpoint %3").args(
                        server.first, module->id, module->endpoint));

                    EXPECT_EQ(module->id.toString(), otherServer.first.toString());
                    EXPECT_EQ(module->endpoint.port,
                        otherServer.second->moduleInstance()->endpoint().port);
                }
                else
                {
                    const auto error = lm("Module %1 failed to discover %2").args(
                        server.first, otherServer.first);

                    #if defined(Q_OS_MAC)
                        // Can join different UDT sockets to the same multicast group on OSX.
                        NX_WARNING(this, error);
                    #else
                        FAIL() << error.toStdString();
                    #endif
                }
            }
        }

        #if defined(Q_OS_MAC)
            EXPECT_GE(totalDiscoveryLinks, m_servers.size() - 1);
        #else
            EXPECT_EQ(m_servers.size() * (m_servers.size() - 1), totalDiscoveryLinks);
        #endif
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
