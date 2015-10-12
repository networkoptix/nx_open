#ifndef MEDIASERVER_API_H
#define MEDIASERVER_API_H

#include <utils/network/socket_common.h>
#include <utils/network/http/httpclient.h>

#include "request_processor.h"

namespace nx {
namespace hpm {

//! Mediaserver API communicating interface
class MediaserverApiIf
        : protected RequestProcessor
{
public:
    MediaserverApiIf( CloudDataProviderIf* cloudData,
                      stun::MessageDispatcher* dispatcher );

    void ping( const ConnectionSharedPtr& connection, stun::Message message );

    //! Pings \a address and verifies if the is the mediaservers with \a expectedId
    virtual void pingServer( const SocketAddress& address, const String& expectedId,
                             std::function< void( SocketAddress, bool ) > onPinged ) = 0;
};

//! Mediaserver API communicating interface over \class nx_http::AsyncHttpClient
class MediaserverApi
        : public MediaserverApiIf
{
public:
    MediaserverApi( CloudDataProviderIf* cloudData,
                    stun::MessageDispatcher* dispatcher );

    virtual void pingServer( const SocketAddress& address, const String& expectedId,
                             std::function< void( SocketAddress, bool ) > onPinged ) override;

private:
    QnMutex m_mutex;
    std::set< nx_http::AsyncHttpClientPtr > m_httpClients;
};

} // namespace hpm
} // namespace nx

#endif // MEDIASERVER_COMMUNICATOR_H
