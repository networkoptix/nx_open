#include "stun_async_client_acceptance_tests.h"

#include <nx/network/system_socket.h>

namespace nx {
namespace network {
namespace stun {
namespace test {

namespace {

class UnconnectableStreamSocket:
    public nx::network::TCPSocket
{
public:
    virtual void connectAsync(
        const SocketAddress& /*addr*/,
        nx::utils::MoveOnlyFunc<void(SystemError::ErrorCode)> handler) override
    {
        post([handler = std::move(handler)]() { handler(SystemError::connectionRefused); });
    }
};

} // namespace

BasicStunAsyncClientAcceptanceTest::~BasicStunAsyncClientAcceptanceTest()
{
    if (m_streamSocketFactoryBak)
        SocketFactory::setCreateStreamSocketFunc(std::move(*m_streamSocketFactoryBak));
}

void BasicStunAsyncClientAcceptanceTest::setSingleShotUnconnectableSocketFactory()
{
    using namespace std::placeholders;

    m_streamSocketFactoryBak = SocketFactory::setCreateStreamSocketFunc(
        std::bind(&BasicStunAsyncClientAcceptanceTest::createUnconnectableStreamSocket, this, _1, _2));
}

std::unique_ptr<AbstractStreamSocket>
BasicStunAsyncClientAcceptanceTest::createUnconnectableStreamSocket(
    bool /*sslRequired*/,
    nx::network::NatTraversalSupport /*natTraversalRequired*/)
{
    QnMutexLocker lock(&m_mutex);

    SocketFactory::setCreateStreamSocketFunc(std::move(*m_streamSocketFactoryBak));
    m_streamSocketFactoryBak.reset();

    return std::make_unique<UnconnectableStreamSocket>();
}

} // namespace test
} // namespace stun
} // namespace network
} // namespace nx
