// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <string>

#include <nx/utils/system_error.h>

namespace nx::utils {

class NamedPipeSocketImpl;

/**
 * Named pipe for IPC.
 */
class NX_UTILS_API NamedPipeSocket
{
    friend class NamedPipeServer;

public:
    NamedPipeSocket();
    virtual ~NamedPipeSocket();

    /**
     * Blocks till connection is complete.
     * @param timeoutMillis if -1, can wait indefinitely.
     */
    SystemError::ErrorCode connectToServerSync(
        const std::string& pipeName, int timeoutMillis = -1);

    /**
     * Writes in synchronous mode.
     */
    SystemError::ErrorCode write(
        const void* buf,
        unsigned int bytesToWrite,
        unsigned int* const bytesWritten);

    /**
     * Reads in synchronous mode.
     */
    SystemError::ErrorCode read(
        void* buf,
        unsigned int bytesToRead,
        unsigned int* const bytesRead,
        int timeoutMs = 3000);

    SystemError::ErrorCode flush();

private:
    NamedPipeSocketImpl* m_impl = nullptr;

    /**
     * NOTE: This initializer is used by NamedPipeServer.
     */
    NamedPipeSocket(NamedPipeSocketImpl* implToUse);
};

} // namespace nx::utils
