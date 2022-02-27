// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include "abstract_stream_socket_acceptor.h"
#include "abstract_socket.h"

namespace nx {
namespace network {

// TODO: #akolesnikov Remove this class when AbstractStreamServerSocket inherits AbstractStreamSocketAcceptor.
class NX_NETWORK_API StreamServerSocketToAcceptorWrapper:
    public AbstractStreamSocketAcceptor
{
    using base_type = AbstractStreamSocketAcceptor;

public:
    StreamServerSocketToAcceptorWrapper(
        std::unique_ptr<AbstractStreamServerSocket> serverSocket);

    virtual void bindToAioThread(aio::AbstractAioThread* aioThread) override;

    virtual void acceptAsync(AcceptCompletionHandler handler) override;
    virtual void cancelIOSync() override;

private:
    std::unique_ptr<AbstractStreamServerSocket> m_serverSocket;

    virtual void stopWhileInAioThread() override;
};

} // namespace network
} // namespace nx
