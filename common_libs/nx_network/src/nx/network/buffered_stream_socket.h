#pragma once

#include <nx/utils/move_only_func.h>

#include "socket_delegate.h"

namespace nx {
namespace network {

/**
 * Stream socket wrapper with internal buffering to extend async functionality.
 */
class NX_NETWORK_API BufferedStreamSocket:
    public StreamSocketDelegate
{
public:
    BufferedStreamSocket(std::unique_ptr<AbstractStreamSocket> socket);

    /**
     * Handler will be called as soon as there is some data ready to recv.
     * @note: is not thread safe (conflicts with recv and recvAsync)
     */
    void catchRecvEvent(nx::utils::MoveOnlyFunc<void(SystemError::ErrorCode)> handler);

    enum class Inject { only, replace, begin, end };

    /**
     * Injects data to internal buffer if we need to pass socket to another processor but read
     * more we could process ourserve.
     * @note: does not affect catchRecvEvent and is not thread safe (conflicts with recv and recvAsync)
     */
    void injectRecvData(Buffer buffer, Inject injectType = Inject::only);

    // TODO: Usefull if we want to reuse connection when user is done with it.
    // void setDoneHandler(nx::utils::MoveOnlyFunc<void(std::unique_ptr<AbstractStreamSocket>)>);

    int recv(void* buffer, unsigned int bufferLen, int flags = 0) override;

    void readSomeAsync(
        nx::Buffer* const buf,
        IoCompletionHandler handler) override;

    virtual QString idForToStringFromPtr() const override;

private:
    std::unique_ptr<AbstractStreamSocket> m_socket;
    Buffer m_internalRecvBuffer;
    nx::utils::MoveOnlyFunc<void(SystemError::ErrorCode)> m_catchRecvEventHandler;

    void triggerCatchRecvEvent(SystemError::ErrorCode resultCode);
};

} // namespace network
} // namespace nx
