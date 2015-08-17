/**********************************************************
* 9 sep 2014
* a.kolesnikov
***********************************************************/

#ifndef LISTENING_PEER_POOL_H
#define LISTENING_PEER_POOL_H

#include "request_processor.h"

namespace nx {
namespace hpm {

//!This class instance keeps information about all currently listening peers, processes STUN requests \a bind, \a connect and sends \a connection_requested indication
/*!
    All methods are reentrant and non-blocking (may be implemented with async fsm)
    \note Is a single-tone
*/
class ListeningPeerPool
        : protected RequestProcessor
{
public:
    ListeningPeerPool( stun::MessageDispatcher* dispatcher );

    void listen( stun::ServerConnection* connection, stun::Message message );
    void connect( stun::ServerConnection* connection, stun::Message message );
};

} // namespace hpm
} // namespace nx

#endif  //LISTENING_PEER_POOL_H
