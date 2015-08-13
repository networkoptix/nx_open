/**********************************************************
* 9 sep 2014
* a.kolesnikov
***********************************************************/

#ifndef LISTENING_PEER_POOL_H
#define LISTENING_PEER_POOL_H

#include <utils/network/stun/server_connection.h>
#include <utils/network/stun/message_dispatcher.h>

namespace nx {
namespace hpm {

class MediaserverApiIf;

//!This class instance keeps information about all currently listening peers, processes STUN requests \a bind, \a connect and sends \a connection_requested indication
/*!
    All methods are reentrant and non-blocking (may be implemented with async fsm)
    \note Is a single-tone
*/
class ListeningPeerPool
{
public:
    ListeningPeerPool( stun::MessageDispatcher* dispatcher,
                       MediaserverApiIf* mediaserverApi );

    bool ping( stun::ServerConnection* connection, stun::Message&& message );
    bool listen( stun::ServerConnection* connection, stun::Message&& message );
    bool connect( stun::ServerConnection* connection, stun::Message&& message );

private:
    bool errorResponse( stun::ServerConnection* connection,
                        stun::Message& request, int code, String reason );

    MediaserverApiIf* m_mediaserverApi;
};

} // namespace hpm
} // namespace nx

#endif  //LISTENING_PEER_POOL_H
