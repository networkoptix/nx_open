/**********************************************************
* Feb 12, 2016
* akolesnikov
***********************************************************/

#pragma once

#include <chrono>

#include <utils/common/stoppable.h>

#include <nx/network/socket_attributes_cache.h>


namespace nx {
namespace network {
namespace cloud {

/** 
 * \note Class instance can be safely freed within connect handler
 */
class AbstractOutgoingTunnelConnection
:
    public QnStoppableAsync
{
public:
    typedef std::function<void(
        SystemError::ErrorCode,
        std::unique_ptr<AbstractStreamSocket>,
        bool /*stillValid*/)> OnNewConnectionHandler;

    virtual ~AbstractOutgoingTunnelConnection() {}

    virtual void establishNewConnection(
        boost::optional<std::chrono::milliseconds> timeout,
        SocketAttributes socketAttributes,
        OnNewConnectionHandler handler) = 0;
};

} // namespace cloud
} // namespace network
} // namespace nx
