// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include <gtest/gtest.h>

#include <nx/network/upnp/upnp_device_searcher.h>
#include <nx/utils/test_support/sync_queue.h>

#include "upnp_port_mapper_mocked.h"

namespace nx::network::upnp::test {

using namespace std::literals::chrono_literals;

const HostAddress kInternalIp("192.168.0.10");
const HostAddress kInitialExternalIp("12.34.56.78");
const HostAddress kChangedExternalIp("34.56.78.91");
constexpr quint16 kMappedPort = 7001;
constexpr quint16 kHttpPort = 80;
constexpr quint16 kExistingExternalPort = 6666;
constexpr auto kWaitTimeout = 30s; //< Below the CI per-test time limit.

class UpnpPortMapper: public ::testing::Test
{
public:
    UpnpPortMapper():
        deviceSearcher(&timerManager, std::make_unique<DeviceSearcherDefaultSettings>()),
        portMapper(&timerManager, kInternalIp, 100ms)
    {
        portMapper.clientMock().changeExternalIp(kInitialExternalIp);
    }

    nx::utils::TimerManager timerManager;
    DeviceSearcher deviceSearcher;
    PortMapperMocked portMapper;
};

TEST_F(UpnpPortMapper, NormalUsage)
{
    // Map 7001 and 80.
    nx::utils::TestSyncQueue<SocketAddress> queue7001;
    EXPECT_TRUE(portMapper.enableMapping(kMappedPort,
        PortMapper::Protocol::tcp,
        [&](SocketAddress info) { queue7001.push(info); }));

    const auto map7001 = queue7001.pop();
    EXPECT_EQ(kInitialExternalIp, map7001.address);
    EXPECT_EQ(portMapper.clientMock().mappings().size(), 1U);

    const auto addr7001 = *portMapper.clientMock().mappings().begin();
    EXPECT_EQ(addr7001.first, std::make_pair(map7001.port, PortMapper::Protocol::tcp));
    EXPECT_EQ(SocketAddress(kInternalIp, kMappedPort), addr7001.second.first);

    nx::utils::TestSyncQueue<SocketAddress> queue80;
    EXPECT_TRUE(portMapper.enableMapping(
        kHttpPort, PortMapper::Protocol::tcp, [&](SocketAddress info) { queue80.push(info); }));

    const auto map80 = queue80.pop();
    EXPECT_EQ(kInitialExternalIp, map80.address);
    EXPECT_GT(map80.port, kHttpPort);
    EXPECT_EQ(portMapper.clientMock().mappingsCount(), 2U);

    // Unmap 7001 and 80.
    EXPECT_TRUE(portMapper.disableMapping(kMappedPort, PortMapper::Protocol::tcp));
    EXPECT_EQ(portMapper.clientMock().mappingsCount(), 1U);

    const auto mapperFor80 = std::make_pair(map80.port, PortMapper::Protocol::tcp);
    const auto addr80 = portMapper.clientMock().mappings()[mapperFor80];
    EXPECT_EQ(SocketAddress(kInternalIp, kHttpPort), addr80.first);

    EXPECT_TRUE(portMapper.disableMapping(kHttpPort, PortMapper::Protocol::tcp));
    EXPECT_EQ(portMapper.clientMock().mappingsCount(), 0U);
}

TEST_F(UpnpPortMapper, ReuseExisting)
{
    // Simulate mapping 6666 -> 192.168.0.10:7001.
    EXPECT_TRUE(portMapper.clientMock().mkMapping(
        std::make_pair(std::make_pair(kExistingExternalPort, PortMapper::Protocol::tcp),
            std::make_pair(SocketAddress(kInternalIp, kMappedPort), QString()))));
    EXPECT_EQ(portMapper.clientMock().mappingsCount(), 1U);

    nx::utils::TestSyncQueue<SocketAddress> queue7001;
    EXPECT_TRUE(portMapper.enableMapping(kMappedPort,
        PortMapper::Protocol::tcp,
        [&](SocketAddress info) { queue7001.push(std::move(info)); }));

    // Existed mapping should be in use.
    EXPECT_EQ(SocketAddress(kInitialExternalIp, kExistingExternalPort), queue7001.pop());
    EXPECT_EQ(portMapper.clientMock().mappingsCount(), 1U);

    // Existed mapping should be removed on request.
    EXPECT_TRUE(portMapper.disableMapping(kMappedPort, PortMapper::Protocol::tcp));
    EXPECT_EQ(portMapper.clientMock().mappingsCount(), 0U);
}

TEST_F(UpnpPortMapper, CheckMappings)
{
    nx::utils::TestSyncQueue<SocketAddress> queue7001;
    EXPECT_TRUE(portMapper.enableMapping(kMappedPort,
        PortMapper::Protocol::tcp,
        [&](SocketAddress info) { queue7001.push(std::move(info)); }));

    EXPECT_EQ(kInitialExternalIp, queue7001.pop().address);
    EXPECT_EQ(portMapper.clientMock().mappingsCount(), 1U);
    portMapper.clientMock().mappings().clear();

    // Wait for mapping to be restored.
    std::this_thread::sleep_for(std::chrono::milliseconds(300));
    EXPECT_EQ(portMapper.clientMock().mappings().size(), 1U);

    // Wait a little longer to be sure nothing got broken.
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    EXPECT_EQ(portMapper.clientMock().mappings().size(), 1U);

    EXPECT_TRUE(portMapper.disableMapping(kMappedPort, PortMapper::Protocol::tcp));
    EXPECT_EQ(portMapper.clientMock().mappingsCount(), 0U);

    // This time mapping wound get restored.
    std::this_thread::sleep_for(std::chrono::milliseconds(300));
    EXPECT_EQ(portMapper.clientMock().mappings().size(), 0U);
}

TEST_F(UpnpPortMapper, ChangeExternalIp)
{
    nx::utils::TestSyncQueue<SocketAddress> queue7001;
    EXPECT_TRUE(portMapper.enableMapping(kMappedPort,
        PortMapper::Protocol::tcp,
        [&](SocketAddress info) { queue7001.push(info); }));

    const SocketAddress initialAddress = queue7001.pop();
    EXPECT_EQ(kInitialExternalIp, initialAddress.address);

    portMapper.clientMock().changeExternalIp(HostAddress());
    // Mapping maintenance can enqueue other notifications concurrently, so select the one caused
    // by each external IP transition instead of assuming it is next in the queue.
    ASSERT_TRUE(queue7001.popIf([expectedAddress = kInitialExternalIp](const SocketAddress& value)
        { return value.address == expectedAddress && value.port == 0; },
        kWaitTimeout));

    portMapper.clientMock().changeExternalIp(kChangedExternalIp);
    ASSERT_TRUE(queue7001.popIf([expectedAddress = kChangedExternalIp](const SocketAddress& value)
        { return value.address == expectedAddress && value.port != 0; },
        kWaitTimeout));
}

} // namespace nx::network::upnp::test
