// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <nx/utils/move_only_func.h>

#include "socket_delegate.h"

namespace nx::network {

/**
 * Stream socket wrapper that reads data from a given buffer first, then switches to regular I/O.
 */
class NX_NETWORK_API BufferedStreamSocket:
    public StreamSocketDelegate
{
public:
    /**
     * After the preReadData is depleted, all I/O is delegated to socket.
     */
    BufferedStreamSocket(
        std::unique_ptr<AbstractStreamSocket> socket,
        nx::Buffer preReadData);

    /**
     * Handler will be called as soon as there is some data ready to be read from the socket.
     * NOTE: is not thread safe (conflicts with recv and recvAsync)
     */
    void catchRecvEvent(nx::utils::MoveOnlyFunc<void(SystemError::ErrorCode)> handler);

    int recv(void* buffer, std::size_t bufferLen, int flags = 0) override;

    void readSomeAsync(
        nx::Buffer* const buf,
        IoCompletionHandler handler) override;

private:
    std::unique_ptr<AbstractStreamSocket> m_socket;
    Buffer m_internalRecvBuffer;
    nx::utils::MoveOnlyFunc<void(SystemError::ErrorCode)> m_catchRecvEventHandler;

    void triggerCatchRecvEvent(SystemError::ErrorCode resultCode);
};

} // namespace nx::network
