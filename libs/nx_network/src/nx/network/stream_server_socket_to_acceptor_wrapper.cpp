#include "stream_server_socket_to_acceptor_wrapper.h"

namespace nx {
namespace network {

StreamServerSocketToAcceptorWrapper::StreamServerSocketToAcceptorWrapper(
    std::unique_ptr<AbstractStreamServerSocket> serverSocket)
    :
    m_serverSocket(std::move(serverSocket))
{
    m_serverSocket->bindToAioThread(getAioThread());
}

void StreamServerSocketToAcceptorWrapper::bindToAioThread(
    aio::AbstractAioThread* aioThread)
{
    base_type::bindToAioThread(aioThread);

    m_serverSocket->bindToAioThread(aioThread);
}

void StreamServerSocketToAcceptorWrapper::acceptAsync(
    AcceptCompletionHandler handler)
{
    m_serverSocket->acceptAsync(std::move(handler));
}

void StreamServerSocketToAcceptorWrapper::cancelIOSync()
{
    m_serverSocket->cancelIOSync();
}

void StreamServerSocketToAcceptorWrapper::stopWhileInAioThread()
{
    m_serverSocket.reset();
}

} // namespace network
} // namespace nx
