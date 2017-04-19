#include "stream_socket_stub.h"

namespace nx {
namespace cloud {
namespace relay {

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

void StreamSocketStub::setConnectionToClosedState()
{
    post(
        [this]()
        {
            if (m_readHandler)
                nx::utils::swapAndCall(m_readHandler, SystemError::noError, 0);
        });
}

} // namespace relay
} // namespace cloud
} // namespace nx
