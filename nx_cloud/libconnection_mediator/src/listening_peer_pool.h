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

    void bind( const ConnectionSharedPtr& connection, stun::Message message );
    void listen( const ConnectionSharedPtr& connection, stun::Message message );
    void connect( const ConnectionSharedPtr& connection, stun::Message message );

protected:
    struct MediaserverPeer
    {
        ConnectionWeakPtr connection;
        std::list< SocketAddress > endpoints;
        bool isListening;

        MediaserverPeer( ConnectionWeakPtr connection_ );
        MediaserverPeer( MediaserverPeer&& peer );
    };

    struct SystemPeers
    {
    public:
        boost::optional< MediaserverPeer& > peer(
                ConnectionWeakPtr connection,
                const RequestProcessor::MediaserverData& mediaserver,
                QnMutex* mutex );

        boost::optional< MediaserverPeer& > search( const String& hostName );

        void clear() { m_peers.clear(); }

    private:
        std::map< String,              // System Id
            std::map< String,          // Server Id
                      MediaserverPeer > > m_peers;
    };

    QnMutex m_mutex;
    SystemPeers m_peers;
};

} // namespace hpm
} // namespace nx

#endif  //LISTENING_PEER_POOL_H
