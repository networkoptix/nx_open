#include <gtest/gtest.h>

#include <nx/utils/log/log.h>
#include <nx/utils/test_support/sync_queue.h>
#include <nx/utils/random.h>
#include <nx/vms/discovery/udp_multicast_finder.h>

namespace nx {
namespace vms {
namespace discovery {
namespace test {

class DiscoveryUdpMulticastFinder: public testing::Test
{
public:
    QnModuleInformationWithAddresses makeModuleInformation() const
    {
        QnModuleInformationWithAddresses module;
        module.id = QnUuid::createUuid();
        module.type = lit("test");
        module.customization = lit("test");
        module.brand = lit("test");
        module.version = QnSoftwareVersion(1, 2, 3, 123);
        module.type = lit("test");
        module.name = lit("test");
        module.port = 7001;
        module.remoteAddresses.insert("127.0.0.1");

        NX_LOGX(lm("New module: %1").arg(module.id), cl_logINFO);
        return module;
    }

    virtual ~DiscoveryUdpMulticastFinder() override { moduleFinder.pleaseStopSync(); }

protected:
    UdpMulticastFinder moduleFinder;
};

TEST_F(DiscoveryUdpMulticastFinder, Base)
{
    moduleFinder.setSendInterval(std::chrono::milliseconds(500));
    moduleFinder.setMulticastEndpoint({
        UdpMulticastFinder::kMulticastEndpoint.address,
        nx::utils::random::number<uint16_t>(6000, 50000)});

    utils::TestSyncMultiQueue<QnModuleInformationWithAddresses, SocketAddress> discoveryQueue;
    moduleFinder.updateInterfaces();
    moduleFinder.listen(discoveryQueue.pusher());

    const auto interfaceCount = (size_t) getLocalIpV4AddressList().size();
    const auto waitForDiscovery =
        [&](const QnModuleInformationWithAddresses& information)
        {
            for (size_t i = 0; i < interfaceCount; ++i)
            {
                const auto actual = discoveryQueue.pop();
                const auto size = discoveryQueue.size();
                EXPECT_EQ(information, actual.first) << size;
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

// Only makes sense to use across real network to test actual multicasts.
TEST_F(DiscoveryUdpMulticastFinder, DISABLED_RealUse)
{
    moduleFinder.updateInterfaces();
    moduleFinder.listen(
        [this](QnModuleInformationWithAddresses module, SocketAddress endpoint)
        {
            NX_LOGX(lm("Found module %1 on %2").args(module.id, endpoint), cl_logINFO);
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
