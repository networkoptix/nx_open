// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <nx/utils/system_error.h>

#include "named_pipe_socket.h"

namespace nx::utils {

class NamedPipeServerImpl;

/**
 * Named pipe for IPC.
 */
class NX_UTILS_API NamedPipeServer
{
public:
    NamedPipeServer();
    virtual ~NamedPipeServer();

    /**
     * Openes pipeName and listenes it for connections.
     * This method returns immediately.
     */
    SystemError::ErrorCode listen(const std::string& pipeName);

    /**
     * Blocks till new connection is accepted, timeout expires or error occurs
     * @param timeoutMillis max time to wait for incoming connection (millis).
     * If -1, wait is infinite.
     * @param sock If return value SystemError::noError,
     * *sock is assigned with newly-created socket. It must be freed by calling party.
     */
    SystemError::ErrorCode accept(NamedPipeSocket** sock, int timeoutMillis = -1);

private:
    NamedPipeServerImpl* m_impl;
};

} // namespace nx::utils
