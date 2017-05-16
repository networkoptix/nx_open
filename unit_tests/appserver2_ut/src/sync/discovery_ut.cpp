#include <gtest/gtest.h>

// TODO: This more like mediaserver_core functionality, so that it has to be in mediaserver_core_ut.

#include <core/resource/resource_type.h>
#include <nx_ec/ec_api.h>
#include <nx/utils/log/log.h>
#include <nx/utils/test_support/module_instance_launcher.h>
#include <nx/utils/test_support/test_options.h>
#include <nx/vms/discovery/manager.h>
#include <test_support/appserver2_process.h>

class DiscoveryTest: public testing::Test
{
protected:
    typedef nx::utils::test::ModuleLauncher<::ec2::Appserver2ProcessPublic> MediaServer;

    void addServer()
    {
        static int instanceCounter = 0;
        const auto tmpDir = QDir::homePath() + "/ec2_discovery_ut.data"
            + QString::number(instanceCounter++);
        QDir(tmpDir).removeRecursively();

        auto server = std::make_unique<MediaServer>();
        server->addArg(lit("--dbFile=%1").arg(tmpDir).toStdString().c_str());
        ASSERT_TRUE(server->startAndWaitUntilStarted());

        const auto module = server->moduleInstance().get();
        initServerData(module);
        NX_ALWAYS(this, lm("Server %1 started at %2").args(
            module->commonModule()->moduleGUID(), server->moduleInstance()->endpoint()));

        module->commonModule()->moduleDiscoveryManager()->start();
        m_servers.emplace(module->commonModule()->moduleGUID(), std::move(server));
    }

    void initServerData(::ec2::Appserver2ProcessPublic* module)
    {
        QnSoftwareVersion version(1, 2, 3, 123);
        const auto connection = module->ecConnection();

        QList<QnResourceTypePtr> resourceTypeList;
        ASSERT_EQ(ec2::ErrorCode::ok, connection->getResourceManager(Qn::kSystemAccess)
             ->getResourceTypesSync(&resourceTypeList));
        qnResTypePool->replaceResourceTypeList(resourceTypeList);

        ec2::ApiMediaServerData serverData;
        auto resTypePtr = qnResTypePool->getResourceTypeByName("Server");
        ASSERT_TRUE(!resTypePtr.isNull());
        serverData.typeId = resTypePtr->getId();
        serverData.id = module->commonModule()->moduleGUID();
        serverData.authKey = QnUuid::createUuid().toString();
        serverData.name = lm("server %1").arg(serverData.id);
        ASSERT_TRUE(!resTypePtr.isNull());
        serverData.typeId = resTypePtr->getId();
        serverData.networkAddresses = module->endpoint().toString();
        serverData.version = version.toString();

        auto serverManager = connection->getMediaServerManager(Qn::kSystemAccess);
        ASSERT_EQ(ec2::ErrorCode::ok, serverManager->saveSync(serverData));

        QnModuleInformation information;
        information.type = QnModuleInformation::nxMediaServerId();
        information.id = serverData.id;
        information.name = serverData.name;
        information.version = version;
        module->commonModule()->setModuleInformation(information);
    }

    void checkVisibility()
    {
        std::this_thread::sleep_for(std::chrono::seconds(10));
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
                    NX_ALWAYS(this, lm("Module %1 discovered %2 with endpoint %3").args(
                        server.first, module->id, module->endpoint));

                    EXPECT_EQ(module->id.toString(), otherServer.first.toString());
                    EXPECT_EQ(module->endpoint.port,
                        otherServer.second->moduleInstance()->endpoint().port);
                }
                else
                {
                    FAIL() << lm("Module %1 failed to discover %2").args(
                        server.first, otherServer.first).toStdString();
                }
            }
        }
    }

private:
    QnStaticCommonModule m_common;
    std::map<QnUuid, std::unique_ptr<MediaServer>> m_servers;
};

TEST_F(DiscoveryTest, main)
{
    for (size_t i = 0; i < 5; ++i)
        addServer();
    checkVisibility();

    for (size_t i = 0; i < 3; ++i)
        addServer();
    checkVisibility();
}
