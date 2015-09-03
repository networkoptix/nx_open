#ifndef MEDIASERVER_API_H
#define MEDIASERVER_API_H

#include <utils/network/socket_common.h>

#include "request_processor.h"

namespace nx {
namespace hpm {

//! Mediaserver API communicating interface
class MediaserverApiIf
        : protected RequestProcessor
{
public:
    MediaserverApiIf( stun::MessageDispatcher* dispatcher );

    void ping( const ConnectionSharedPtr& connection, stun::Message message );

    //! Pings \a address and verifies if the is the mediaservers with \a expectedId
    virtual bool pingServer( const SocketAddress& address,
                             const String& expectedId ) = 0;
};

//! Mediaserver API communicating interface over \class nx_http::AsyncHttpClient
class MediaserverApi
        : public MediaserverApiIf
{
public:
    MediaserverApi( stun::MessageDispatcher* dispatcher );

    virtual bool pingServer( const SocketAddress& address,
                             const String& expectedId ) override;
};

} // namespace hpm
} // namespace nx

#endif // MEDIASERVER_COMMUNICATOR_H
