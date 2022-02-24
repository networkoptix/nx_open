// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <nx/network/abstract_stream_socket_acceptor.h>

namespace nx::network::cloud {

class AbstractConnectionAcceptor:
    public AbstractStreamSocketAcceptor
{
public:
    /**
     * @return Ready-to-use connection from internal listen queue.
     *   nullptr if no connections available.
     */
    virtual std::unique_ptr<AbstractStreamSocket> getNextSocketIfAny() = 0;
};

} // namespace nx::network::cloud
