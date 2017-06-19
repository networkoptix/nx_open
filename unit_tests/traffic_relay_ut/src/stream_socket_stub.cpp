#include "stream_socket_stub.h"

#include <thread>

namespace nx {
namespace cloud {
namespace relay {
namespace test {

StreamSocketStub::StreamSocketStub():
    base_type(&m_delegatee)
{
    setNonBlockingMode(true);
}

void StreamSocketStub::readSomeAsync(
    nx::Buffer* const /*buffer*/,
    std::function<void(SystemError::ErrorCode, size_t)> handler)
{
    post([this, handler = std::move(handler)]() { m_readHandler = std::move(handler); });
}

void StreamSocketStub::sendAsync(
    const nx::Buffer& buffer,
    std::function<void(SystemError::ErrorCode, size_t)> handler)
{
    post(
        [this, &buffer, handler = std::move(handler)]()
        {
            m_reflectingPipeline.write(buffer.data(), buffer.size());
            handler(SystemError::noError, buffer.size());
        });
}

SocketAddress StreamSocketStub::getForeignAddress() const
{
    return m_foreignAddress;
}

bool StreamSocketStub::setKeepAlive(boost::optional<KeepAliveOptions> info)
{
    m_keepAliveOptions = info;
    return true;
}

bool StreamSocketStub::getKeepAlive(boost::optional<KeepAliveOptions>* result) const
{
    *result = m_keepAliveOptions;
    return true;
}

QByteArray StreamSocketStub::read()
{
    while (m_reflectingPipeline.totalBytesThrough() == 0)
        std::this_thread::yield();
    return m_reflectingPipeline.readAll();
}

void StreamSocketStub::setConnectionToClosedState()
{
    post(
        [this]()
        {
            if (m_readHandler)
                nx::utils::swapAndCall(m_readHandler, SystemError::noError, 0);
        });
}

void StreamSocketStub::setForeignAddress(const SocketAddress& endpoint)
{
    m_foreignAddress = endpoint;
}

} // namespace test
} // namespace relay
} // namespace cloud
} // namespace nx
