/**********************************************************
* 9 sep 2014
* a.kolesnikov
***********************************************************/

#ifndef LISTENING_PEER_POOL_H
#define LISTENING_PEER_POOL_H

#include "db/registered_systems_data_manager.h"
#include "stun/stun_server_connection.h"


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

    /*!
        \note Follows \a STUNMessageDispatcher::MessageProcessorType signature
    */
    bool processBindRequest( StunServerConnection* connection, nx_stun::Message&& message );
    /*!
        \note Follows \a STUNMessageDispatcher::MessageProcessorType signature
    */
    bool processConnectRequest( StunServerConnection* connection, nx_stun::Message&& message );

    static ListeningPeerPool* instance();
};

#endif  //LISTENING_PEER_POOL_H
