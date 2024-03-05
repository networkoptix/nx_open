// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include <gtest/gtest.h>

#include <nx/fusion/serialization/json.h>
#include <nx/network/app_info.h>
#include <nx/network/cloud/cloud_connect_controller.h>
#include <nx/network/socket_global.h>
#include <nx/utils/log/log.h>
#include <nx/utils/random.h>
#include <nx/utils/test_support/sync_queue.h>
#include <nx/utils/test_support/test_options.h>
#include <nx/vms/discovery/udp_multicast_finder.h>
#include <nx/vms/api/data/module_information.h>

namespace nx {
namespace vms {
namespace discovery {
namespace test {

// NOTE: Used as multicast endpoint to avoid collisions with other tests and real systems. Port
// is dynamically allocated and IP address should differ from default in real systems.
const nx::network::SocketAddress kMulticastEndpoint("239.255.11.12:0");

class DiscoveryUdpMulticastFinder: public testing::Test
{
public:
    DiscoveryUdpMulticastFinder(): systemId(nx::Uuid::createUuid()) {}

    nx::vms::api::ModuleInformationWithAddresses makeModuleInformation() const
    {
        nx::vms::api::ModuleInformationWithAddresses module;
        module.id = nx::Uuid::createUuid();
        module.localSystemId = systemId;
        module.type = "test";
        module.customization = "test";
        module.brand = "test";
        module.version = nx::utils::SoftwareVersion(1, 2, 3, 123);
        module.type = "test";
        module.name = "test";
        module.port = 7001;
        module.remoteAddresses.insert("127.0.0.1");
        module.realm = nx::network::AppInfo::realm().c_str();
        module.cloudHost = nx::network::SocketGlobals::cloud().cloudHost().c_str();

        NX_INFO(this, nx::format("New module: %1").arg(module.id));
        return module;
    }

    virtual ~DiscoveryUdpMulticastFinder() override { moduleFinder.pleaseStopSync(); }

protected:
    UdpMulticastFinder moduleFinder;
    nx::Uuid systemId;
};

TEST_F(DiscoveryUdpMulticastFinder, Base)
{
    utils::TestSyncMultiQueue<
        nx::vms::api::ModuleInformationWithAddresses, nx::network::SocketAddress> discoveryQueue;
    moduleFinder.setSendInterval(std::chrono::milliseconds(500));
    moduleFinder.setMulticastEndpoint(kMulticastEndpoint);
    moduleFinder.updateInterfaces();
    moduleFinder.listen(
        [this, &discoveryQueue](const nx::vms::api::ModuleInformationWithAddresses& module,
            const nx::network::SocketAddress& endpoint)
        {
            if (module.localSystemId == systemId)
                discoveryQueue.push(module, endpoint);
        });

    const auto interfaces = nx::network::getLocalIpV4AddressList();
    const auto removeInterface =
        [&](nx::vms::api::ModuleInformationWithAddresses* information)
        {
            for (const auto& interface: interfaces)
            {
                if (information->remoteAddresses.remove(interface))
                    return true;
            }
            return false;
        };
    const auto waitForDiscovery =
        [&](const nx::vms::api::ModuleInformationWithAddresses& information)
        {
            for (int i = 0; i < interfaces.size(); ++i)
            {
                auto notification = discoveryQueue.pop().first;
                EXPECT_TRUE(removeInterface(&notification)) << i;
                EXPECT_EQ(QJson::serialized(information), QJson::serialized(notification)) << i;
            }
        };

    const auto information = makeModuleInformation();
    moduleFinder.multicastInformation(information);
    waitForDiscovery(information);

    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    ASSERT_TRUE(discoveryQueue.isEmpty()); //< Too early.

    std::this_thread::sleep_for(std::chrono::milliseconds(1));
    waitForDiscovery(information); //< Multicasted again.

    moduleFinder.pleaseStopSync();
    std::this_thread::sleep_for(std::chrono::seconds(1));
    ASSERT_TRUE(discoveryQueue.isEmpty()); //< Disabled.

    moduleFinder.updateInterfaces();
    waitForDiscovery(information); //< Multicasted again.

    const auto information2 = makeModuleInformation();
    moduleFinder.multicastInformation(information2);
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    discoveryQueue.clear();
    waitForDiscovery(information2); //< Replaced data.
}

TEST_F(DiscoveryUdpMulticastFinder, UpdateInterfacesAndModuleInformation)
{
    moduleFinder.setSendInterval(std::chrono::milliseconds(20));
    moduleFinder.setUpdateInterfacesInterval(std::chrono::milliseconds(50));
    moduleFinder.setMulticastEndpoint(kMulticastEndpoint);
    moduleFinder.updateInterfaces();
    moduleFinder.listen(
        [this](nx::vms::api::ModuleInformationWithAddresses module,
            nx::network::SocketAddress endpoint)
        {
            NX_VERBOSE(this, nx::format("Found module %1 on %2").args(module.id, endpoint));
        });

    const auto count = nx::TestOptions::applyLoadMode(100);
    for (auto i = 0; i < count; ++i)
    {
        moduleFinder.multicastInformation(makeModuleInformation());
        std::this_thread::sleep_for(std::chrono::milliseconds(nx::utils::random::number(10, 50)));
    }
}

// Only makes sense to use across real network to test actual multicasts.
TEST_F(DiscoveryUdpMulticastFinder, DISABLED_RealUse)
{
    moduleFinder.updateInterfaces();
    moduleFinder.listen(
        [this](nx::vms::api::ModuleInformationWithAddresses module,
            nx::network::SocketAddress endpoint)
        {
            NX_INFO(this, nx::format("Found module %1 on %2").args(module.id, endpoint));
        });

    for (;;)
    {
        moduleFinder.multicastInformation(makeModuleInformation());
        std::this_thread::sleep_for(std::chrono::seconds(10));
    }
}

} // namespace test
} // namespace discovery
} // namespace vms
} // namespace nx
