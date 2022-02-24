// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "stream_server_socket_to_acceptor_wrapper.h"

namespace nx {
namespace network {

StreamServerSocketToAcceptorWrapper::StreamServerSocketToAcceptorWrapper(
    std::unique_ptr<AbstractStreamServerSocket> serverSocket)
    :
    m_serverSocket(std::move(serverSocket))
{
    bindToAioThread(m_serverSocket->getAioThread());
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
    m_serverSocket->pleaseStopSync();
}

} // namespace network
} // namespace nx
