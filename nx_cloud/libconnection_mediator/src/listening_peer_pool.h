/**********************************************************
* 9 sep 2014
* a.kolesnikov
***********************************************************/

#ifndef LISTENING_PEER_POOL_H
#define LISTENING_PEER_POOL_H

#include <functional>

#include <nx/network/cloud/data/connection_result_data.h>
#include <nx/network/cloud/data/resolve_data.h>
#include <nx/network/cloud/data/result_code.h>
#include <utils/serialization/lexical.h>

#include "request_processor.h"
#include "server/connection.h"


namespace nx {
namespace hpm {

class MessageDispatcher;

//!This class instance keeps information about all currently listening peers, processes STUN requests \a bind, \a connect and sends \a connection_requested indication
/*!
    All methods are reentrant and non-blocking (may be implemented with async fsm)
    \note Is a single-tone
*/
class ListeningPeerPool
        : protected RequestProcessor
{
public:
    ListeningPeerPool( AbstractCloudDataProvider* cloudData,
                       MessageDispatcher* dispatcher );

    void bind(
        ConnectionStrongRef connection,
        stun::Message message);
    void listen(
        ConnectionStrongRef connection,
        stun::Message message);
    void resolve(
        ConnectionStrongRef connection,
        api::ResolveRequest request,
        std::function<void(api::ResultCode, api::ResolveResponse)> completionHandler);
    void connect(
        ConnectionStrongRef connection,
        stun::Message message);

    void connectionResult(
        ConnectionStrongRef connection,
        api::ConnectionResultRequest request,
        std::function<void(api::ResultCode)> completionHandler);

protected:
    struct MediaserverPeer
    {
        ConnectionWeakRef connection;
        std::list< SocketAddress > endpoints;
        bool isListening;

        MediaserverPeer( ConnectionWeakRef connection_ );
        MediaserverPeer( MediaserverPeer&& peer );
    };

    struct SystemPeers
    {
    public:
        boost::optional< MediaserverPeer& > peer(
                ConnectionWeakRef connection,
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
