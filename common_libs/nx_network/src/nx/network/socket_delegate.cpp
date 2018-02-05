#include "socket_delegate.h"

namespace nx {
namespace network {

//-------------------------------------------------------------------------------------------------
// StreamSocketDelegate

StreamSocketDelegate::StreamSocketDelegate(AbstractStreamSocket* target):
    base_type(target)
{
}

bool StreamSocketDelegate::setNoDelay(bool value)
{
    return m_target->setNoDelay(value);
}

bool StreamSocketDelegate::getNoDelay(bool* value) const
{
    return m_target->getNoDelay(value);
}

bool StreamSocketDelegate::toggleStatisticsCollection(bool val)
{
    return m_target->toggleStatisticsCollection(val);
}

bool StreamSocketDelegate::getConnectionStatistics(StreamSocketInfo* info)
{
    return m_target->getConnectionStatistics(info);
}

bool StreamSocketDelegate::setKeepAlive(boost::optional< KeepAliveOptions > info)
{
    return m_target->setKeepAlive(info);
}

bool StreamSocketDelegate::getKeepAlive(boost::optional< KeepAliveOptions >* result) const
{
    return m_target->getKeepAlive(result);
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

void StreamServerSocketDelegate::pleaseStopSync(bool assertIfCalledUnderLock)
{
    m_target->pleaseStopSync(assertIfCalledUnderLock);
}

bool StreamServerSocketDelegate::listen(int backlog)
{
    return m_target->listen(backlog);
}

AbstractStreamSocket* StreamServerSocketDelegate::accept()
{
    return m_target->accept();
}

void StreamServerSocketDelegate::acceptAsync(AcceptCompletionHandler handler)
{
    return m_target->acceptAsync(std::move(handler));
}

void StreamServerSocketDelegate::cancelIOAsync(nx::utils::MoveOnlyFunc<void()> handler)
{
    return m_target->cancelIOAsync(std::move(handler));
}

void StreamServerSocketDelegate::cancelIOSync()
{
    return m_target->cancelIOSync();
}

} // namespace network
} // namespace nx
