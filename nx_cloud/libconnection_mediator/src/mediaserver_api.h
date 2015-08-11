#ifndef MEDIASERVER_API_H
#define MEDIASERVER_API_H

#include <utils/network/socket_common.h>

namespace nx {
namespace hpm {

//! Mediaserver API communicating interface
class MediaserverApiIf
{
public:
    virtual ~MediaserverApiIf();

    //! Pings \a address and verifies if the is the mediaservers with \a expectedId
    virtual bool ping( const SocketAddress& address, const QnUuid& expectedId ) = 0;
};

//! Mediaserver API communicating interface over \class nx_http::AsyncHttpClient
class MediaserverApi
        : public MediaserverApiIf
{
public:
    bool ping( const SocketAddress& address, const QnUuid& expectedId ) override;
};

} // namespace hpm
} // namespace nx

#endif // MEDIASERVER_COMMUNICATOR_H
