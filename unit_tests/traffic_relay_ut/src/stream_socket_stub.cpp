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

    m_timer.bindToAioThread(getAioThread());
}

StreamSocketStub::~StreamSocketStub()
{
    m_timer.pleaseStopSync();
}

void StreamSocketStub::post(nx::utils::MoveOnlyFunc<void()> func)
{
    if (m_postDelay)
        m_timer.start(*m_postDelay, std::move(func));
    else
        base_type::post(std::move(func));
}

void StreamSocketStub::bindToAioThread(nx::network::aio::AbstractAioThread* aioThread)
{
    base_type::bindToAioThread(aioThread);
    m_timer.bindToAioThread(aioThread);
}

void StreamSocketStub::readSomeAsync(
    nx::Buffer* const buffer,
    std::function<void(SystemError::ErrorCode, size_t)> handler)
{
    base_type::post(
        [this, buffer, handler = std::move(handler)]()
        {
            m_readBuffer = buffer;
            m_readHandler = std::move(handler);
        });
}

void StreamSocketStub::sendAsync(
    const nx::Buffer& buffer,
    std::function<void(SystemError::ErrorCode, size_t)> handler)
{
    base_type::post(
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
    base_type::post(
        [this]()
        {
            m_readBuffer->append("h");
            if (m_readHandler)
                nx::utils::swapAndCall(m_readHandler, SystemError::noError, 0);
        });
}

void StreamSocketStub::setForeignAddress(const SocketAddress& endpoint)
{
    m_foreignAddress = endpoint;
}

void StreamSocketStub::setPostDelay(boost::optional<std::chrono::milliseconds> postDelay)
{
    m_postDelay = postDelay;
}

} // namespace test
} // namespace relay
} // namespace cloud
} // namespace nx
