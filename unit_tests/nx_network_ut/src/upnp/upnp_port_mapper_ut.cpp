#include <gtest/gtest.h>

#include <nx/network/upnp/upnp_device_searcher.h>
#include <nx/utils/test_support/sync_queue.h>

#include "upnp_port_mapper_mocked.h"

namespace nx {
namespace network {
namespace upnp {
namespace test {

class UpnpPortMapper: public ::testing::Test
{
public:
    UpnpPortMapper():
        deviceSearcher(deviceSearcherSettings),
        portMapper(lit("192.168.0.10"), 100)
    {
        portMapper.clientMock().changeExternalIp(lit("12.34.56.78"));
    }

    nx::utils::TimerManager timerManager;
    DeviceSearcherDefaultSettings deviceSearcherSettings;
    DeviceSearcher deviceSearcher;
    PortMapperMocked portMapper;
};

TEST_F(UpnpPortMapper, NormalUsage)
{
    // Map 7001 and 80.
    nx::utils::TestSyncQueue<SocketAddress> queue7001;
    EXPECT_TRUE(portMapper.enableMapping(7001, PortMapper::Protocol::TCP,
        [&](SocketAddress info) {
            queue7001.push(info);
        }));

    const auto map7001 = queue7001.pop();
    EXPECT_EQ(map7001.address.toString(), lit("12.34.56.78"));
    EXPECT_EQ(portMapper.clientMock().mappings().size(), 1U);

    const auto addr7001 = *portMapper.clientMock().mappings().begin();
    EXPECT_EQ(addr7001.first, std::make_pair(map7001.port, PortMapper::Protocol::TCP));
    EXPECT_EQ(addr7001.second.first.toString(), lit("192.168.0.10:7001"));

    nx::utils::TestSyncQueue<SocketAddress> queue80;
    EXPECT_TRUE(portMapper.enableMapping(80, PortMapper::Protocol::TCP,
        [&](SocketAddress info) { queue80.push(info); }));

    const auto map80 = queue80.pop();
    EXPECT_EQ(map80.address.toString(), lit("12.34.56.78"));
    EXPECT_GT(map80.port, 80);
    EXPECT_EQ(portMapper.clientMock().mappingsCount(), 2U);

    // Unmap 7001 and 80.
    EXPECT_TRUE(portMapper.disableMapping(7001, PortMapper::Protocol::TCP));
    EXPECT_EQ(portMapper.clientMock().mappingsCount(), 1U);

    const auto mapperFor80 = std::make_pair(map80.port, PortMapper::Protocol::TCP);
    const auto addr80 = portMapper.clientMock().mappings()[mapperFor80];
    EXPECT_EQ(addr80.first.toString(), lit("192.168.0.10:80"));

    EXPECT_TRUE(portMapper.disableMapping(80, PortMapper::Protocol::TCP));
    EXPECT_EQ(portMapper.clientMock().mappingsCount(), 0U);
}

TEST_F(UpnpPortMapper, ReuseExisting)
{
    // Simulate mapping 6666 -> 192.168.0.10:7001.
    EXPECT_TRUE(portMapper.clientMock().mkMapping(std::make_pair(
        std::make_pair(6666, PortMapper::Protocol::TCP),
        std::make_pair(SocketAddress(lit("192.168.0.10"), 7001), QString()))));
    EXPECT_EQ(portMapper.clientMock().mappingsCount(), 1U);

    nx::utils::TestSyncQueue<SocketAddress> queue7001;
    EXPECT_TRUE(portMapper.enableMapping(7001, PortMapper::Protocol::TCP,
        [&](SocketAddress info) { queue7001.push(std::move(info)); }));

    // Existed mapping should be in use.
    EXPECT_EQ(queue7001.pop().toString(), lit("12.34.56.78:6666"));
    EXPECT_EQ(portMapper.clientMock().mappingsCount(), 1U);

    // Existed mapping should be removed on request.
    EXPECT_TRUE(portMapper.disableMapping(7001, PortMapper::Protocol::TCP));
    EXPECT_EQ(portMapper.clientMock().mappingsCount(), 0U);
}

TEST_F(UpnpPortMapper, CheckMappings)
{
    nx::utils::TestSyncQueue<SocketAddress> queue7001;
    EXPECT_TRUE(portMapper.enableMapping(7001, PortMapper::Protocol::TCP,
        [&](SocketAddress info) { queue7001.push(std::move(info)); }));

    EXPECT_EQ(queue7001.pop().address.toString(), lit("12.34.56.78"));
    EXPECT_EQ(portMapper.clientMock().mappingsCount(), 1U);
    portMapper.clientMock().mappings().clear();

    // Wait for mapping to be restored.
    std::this_thread::sleep_for(std::chrono::milliseconds(300));
    EXPECT_EQ(portMapper.clientMock().mappings().size(), 1U);

    // Wait a little longer to be sure nothing got broken.
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    EXPECT_EQ(portMapper.clientMock().mappings().size(), 1U);

    EXPECT_TRUE(portMapper.disableMapping(7001, PortMapper::Protocol::TCP));
    EXPECT_EQ(portMapper.clientMock().mappingsCount(), 0U);

    // This time mapping wount get restored.
    std::this_thread::sleep_for(std::chrono::milliseconds(300));
    EXPECT_EQ(portMapper.clientMock().mappings().size(), 0U);
}

TEST_F(UpnpPortMapper, ChangeExternalIp)
{
     nx::utils::TestSyncQueue<SocketAddress> queue7001;
     EXPECT_TRUE(portMapper.enableMapping(7001, PortMapper::Protocol::TCP,
        [&](SocketAddress info) { queue7001.push(info); }));

     EXPECT_EQ(queue7001.pop().address.toString(), lit("12.34.56.78"));

     portMapper.clientMock().changeExternalIp(HostAddress());
     EXPECT_EQ(queue7001.pop().toString(), lit("12.34.56.78"));

     portMapper.clientMock().changeExternalIp(lit("34.56.78.91"));
     EXPECT_EQ(queue7001.pop().address.toString(), lit("34.56.78.91"));
}

TEST_F(UpnpPortMapper, DISABLED_RealRouter)
{
    PortMapper portMapper(true, 10000);
    EXPECT_TRUE(portMapper.enableMapping(7001, PortMapper::Protocol::TCP, [&](SocketAddress) {}));
    QThread::sleep(60 * 60);
}

} // namespace test
} // namespace upnp
} // namespace network
} // namespace nx
