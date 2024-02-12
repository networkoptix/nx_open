// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <map>

#include <QtCore/QObject>

#include <nx/network/http/auth_cache.h>
#include <nx/p2p/transaction_filter.h>

#include "transaction.h"

namespace ec2 {

    static const nx::Uuid kCloudPeerId("674BAFD7-4EEC-4BBA-84AA-A1BAEA7FC6DB");

    /**
     * This base class is used for both old TransactionTransport and new P2pConnection classes.
     */
    class QnAbstractTransactionTransport: public QObject
    {
    public:
        virtual const nx::vms::api::PeerData& localPeer() const = 0;
        virtual const nx::vms::api::PeerData& remotePeer() const = 0;
        virtual nx::utils::Url remoteAddr() const = 0;
        virtual bool isIncoming() const = 0;
        virtual std::multimap<QString, QString> httpQueryParams() const = 0;

        template<typename Transaction>
        // requires std::is_base_of<QnAbstractTransaction, Transaction>
        nx::p2p::FilterResult shouldTransactionBeSentToPeer(const Transaction& transaction)
        {
            using namespace nx::vms;

            if (remotePeer().peerType == api::PeerType::oldMobileClient &&
                skipTransactionForMobileClient(transaction.command))
            {
                return nx::p2p::FilterResult::deny;
            }
            else if (remotePeer().peerType == api::PeerType::oldServer)
            {
                return nx::p2p::FilterResult::deny;
            }
            else if (transaction.isLocal() && !remotePeer().isClient())
            {
                return nx::p2p::FilterResult::deny;
            }

            if (transaction.command == ::ec2::ApiCommand::tranSyncRequest ||
                transaction.command == ::ec2::ApiCommand::tranSyncResponse ||
                transaction.command == ::ec2::ApiCommand::tranSyncDone)
            {
                // Always sending "special" transactions. E.g., connection handshake.
                return nx::p2p::FilterResult::allow;
            }

            // TODO: #akolesnikov Replace the condition with "is filter present?"
            if (remotePeer().peerType == api::PeerType::cloudServer)
                return m_cloudTransactionFilter.match(transaction);

            return nx::p2p::FilterResult::allow;
        }

    private:
        nx::p2p::CloudTransactionFilter m_cloudTransactionFilter;

    private:
        static bool skipTransactionForMobileClient(ApiCommand::Value command);
    };

} // namespace ec2
