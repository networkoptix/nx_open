#include "socket_delegate.h"

namespace nx {
namespace network {

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
