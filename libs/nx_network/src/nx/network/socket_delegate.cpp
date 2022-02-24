// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "socket_delegate.h"

#include "socket_global.h"

namespace nx {
namespace network {

namespace detail {

void recordSocketDelegateCreation(void* ptr)
{
    SocketGlobals::instance().allocationAnalyzer().recordObjectCreation(ptr);
}

void recordSocketDelegateDestruction(void* ptr)
{

    SocketGlobals::instance().allocationAnalyzer().recordObjectDestruction(ptr);
}

} // namespace detail

StreamSocketDelegate::StreamSocketDelegate(AbstractStreamSocket* target):
    base_type(target)
{
}

//-------------------------------------------------------------------------------------------------
// StreamServerSocketDelegate

StreamServerSocketDelegate::StreamServerSocketDelegate(AbstractStreamServerSocket* target):
    base_type(target)
{
}

void StreamServerSocketDelegate::pleaseStop(nx::utils::MoveOnlyFunc<void()> handler)
{
    m_target->pleaseStop(std::move(handler));
}

void StreamServerSocketDelegate::pleaseStopSync()
{
    m_target->pleaseStopSync();
}

bool StreamServerSocketDelegate::listen(int backlog)
{
    return m_target->listen(backlog);
}

std::unique_ptr<AbstractStreamSocket> StreamServerSocketDelegate::accept()
{
    return m_target->accept();
}

void StreamServerSocketDelegate::acceptAsync(AcceptCompletionHandler handler)
{
    return m_target->acceptAsync(std::move(handler));
}

void StreamServerSocketDelegate::cancelIoInAioThread()
{
    return m_target->cancelIOSync();
}

} // namespace network
} // namespace nx
