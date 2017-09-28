#pragma once

#include <list>
#include <memory>
#include <string>

#include <nx/network/abstract_socket.h>
#include <nx/network/aio/abstract_async_channel.h>
#include <nx/network/aio/async_channel_bridge.h>
#include <nx/utils/move_only_func.h>
#include <nx/utils/thread/mutex.h>

namespace nx {
namespace cloud {
namespace relay {
namespace controller {

struct RelayConnectionData
{
    std::unique_ptr<network::aio::AbstractAsyncChannel> connection;
    std::string peerId;
};

class AbstractTrafficRelay
{
public:
    virtual ~AbstractTrafficRelay() = default;

    virtual void startRelaying(
        RelayConnectionData clientConnection,
        RelayConnectionData serverConnection) = 0;
};

class TrafficRelay:
    public AbstractTrafficRelay
{
public:
    virtual ~TrafficRelay() override;

    virtual void startRelaying(
        RelayConnectionData clientConnection,
        RelayConnectionData serverConnection) override;

private:
    struct RelaySession
    {
        std::unique_ptr<network::aio::AsyncChannelBridge> channelBridge;
        std::string clientPeerId;
        std::string serverPeerId;
    };

    std::list<RelaySession> m_relaySessions;
    mutable QnMutex m_mutex;
    bool m_terminated = false;

    void onRelaySessionFinished(std::list<RelaySession>::iterator sessionIter);
};

} // namespace controller
} // namespace relay
} // namespace cloud
} // namespace nx
