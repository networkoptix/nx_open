/**********************************************************
* 9 sep 2014
* a.kolesnikov
***********************************************************/

#ifndef LISTENING_PEER_POOL_H
#define LISTENING_PEER_POOL_H

#include "db/registered_systems_data_manager.h"
#include "stun/stun_server_connection.h"

namespace nx {
namespace hpm {

class STUNMessageDispatcher;

//!This class instance keeps information about all currently listening peers, processes STUN requests \a bind, \a connect and sends \a connection_requested indication
/*!
    All methods are reentrant and non-blocking (may be implemented with async fsm)
    \note Is a single-tone
*/
class ListeningPeerPool
{
public:
    ListeningPeerPool();
    virtual ~ListeningPeerPool();

    bool registerRequestProcessors( STUNMessageDispatcher& dispatcher );

    bool ping( StunServerConnection* connection, stun::Message&& message );
    bool listen( StunServerConnection* connection, stun::Message&& message );
    bool connect( StunServerConnection* connection, stun::Message&& message );

    static ListeningPeerPool* instance();

private:
    bool errorResponse( StunServerConnection* connection,
                        stun::Message& request, int code, String reason );
};

} // namespace hpm
} // namespace nx

#endif  //LISTENING_PEER_POOL_H
