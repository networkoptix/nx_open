#pragma once

#include <gmock/gmock.h>

#include <mediaserver_endpoint_tester.h>

#include "custom_printers.h"

namespace nx {
namespace hpm {
namespace test {

class CloudDataProviderMock:
    public AbstractCloudDataProvider
{
public:
    MOCK_CONST_METHOD1(getSystem, std::optional< System >(const String&));

    CloudDataProviderMock()
    {
        ON_CALL(*this, getSystem(::testing::_))
            .WillByDefault(::testing::Return(std::nullopt));
    }

    inline void expect_getSystem(
        const String& systemId,
        const String& authKey,
        size_t count = 1)
    {
        EXPECT_CALL(*this, getSystem(systemId))
            .Times(count).WillRepeatedly(::testing::Return(System(authKey, true)));
    };
};

class MediaserverEndpointTesterMock:
    public MediaserverEndpointTesterBase
{
public:
    MOCK_METHOD3(pingServer, void(const nx::network::SocketAddress&, const String&,
        std::function< void(nx::network::SocketAddress, bool) >));

    MediaserverEndpointTesterMock(AbstractCloudDataProvider* cloudData):
        MediaserverEndpointTesterBase(cloudData)
    {
    }

    inline void expect_pingServer(
        const nx::network::SocketAddress& address,
        const String& serverId,
        bool result,
        size_t count = 1)
    {
        EXPECT_CALL(*this, pingServer(address, serverId, ::testing::_))
            .Times(count).WillRepeatedly(::testing::Invoke(
                [=](const nx::network::SocketAddress&, const String&,
                    std::function< void(nx::network::SocketAddress, bool) > onPinged)
        { onPinged(address, result); }));
    }
};

} // namespace test
} // namespace hpm
} // namespace nx
