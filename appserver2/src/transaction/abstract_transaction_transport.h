#pragma once

#include <QtCore/QObject>
#include <nx_ec/data/api_peer_data.h>
#include "transaction.h"
#include <nx/network/http/auth_cache.h>

namespace ec2 {

    static const QnUuid kCloudPeerId(lit("674BAFD7-4EEC-4BBA-84AA-A1BAEA7FC6DB"));

    /**
     * This base class is used for both old TransactionTransport and new P2pConnection classes.
     */
    class QnAbstractTransactionTransport: public QObject
    {
    public:
        virtual const ec2::ApiPeerData& localPeer() const = 0;
        virtual const ec2::ApiPeerData& remotePeer() const = 0;
        virtual nx::utils::Url remoteAddr() const = 0;
        virtual bool isIncoming() const = 0;
        virtual nx::network::http::AuthInfoCache::AuthorizationCacheItem authData() const = 0;

        bool shouldTransactionBeSentToPeer(const QnAbstractTransaction& transaction);
    };

} // namespace ec2
