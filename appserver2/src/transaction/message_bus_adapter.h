#pragma once

#include <QtCore/QThread>

#include "abstract_transaction_message_bus.h"
#include <transaction/transaction_message_bus.h>
#include <nx/p2p/p2p_message_bus.h>

namespace ec2
{

    enum class MessageBusType
    {
        None,
        LegacyMode,
        P2pMode
    };

    class TransactionMessageBusAdapter: public AbstractTransactionMessageBus
    {
        Q_OBJECT
    public:
        TransactionMessageBusAdapter(
            detail::QnDbManager* db,
            Qn::PeerType peerType,
            QnCommonModule* commonModule,
            QnJsonTransactionSerializer* jsonTranSerializer,
            QnUbjsonTransactionSerializer* ubjsonTranSerializer
        );

        void init(MessageBusType value);

        template <typename T> T dynamicCast() { return dynamic_cast<T> (m_bus.get()); }

        virtual ~TransactionMessageBusAdapter() {}

        virtual void start() override;
        virtual void stop() override;

        virtual QSet<QnUuid> directlyConnectedClientPeers() const override;

        virtual QnUuid routeToPeerVia(const QnUuid& dstPeer, int* distance) const override;
        virtual int distanceToPeer(const QnUuid& dstPeer) const override;

        virtual void addOutgoingConnectionToPeer(const QnUuid& id, const nx::utils::Url& url) override;
        virtual void removeOutgoingConnectionFromPeer(const QnUuid& id) override;

        virtual void dropConnections() override;

        virtual QVector<QnTransportConnectionInfo> connectionsInfo() const override;

        virtual void setHandler(ECConnectionNotificationManager* handler) override;
        virtual void removeHandler(ECConnectionNotificationManager* handler) override;

        virtual QnJsonTransactionSerializer* jsonTranSerializer() const override;
        virtual QnUbjsonTransactionSerializer* ubjsonTranSerializer() const override;

        virtual ConnectionGuardSharedState* connectionGuardSharedState() override;
        virtual detail::QnDbManager* getDb() const override;

        virtual void setTimeSyncManager(TimeSynchronizationManager* timeSyncManager) override;
    public:
        template<class T>
        void sendTransaction(
            const QnTransaction<T>& tran)
        {
            if (auto p2pBus = dynamicCast<nx::p2p::MessageBus*>())
                p2pBus->sendTransaction(tran);
            else if (auto msgBus = dynamicCast<QnTransactionMessageBus*>())
                msgBus->sendTransaction(tran);
        }

        template<class T>
        void sendTransaction(
            const QnTransaction<T>& tran,
            const QnPeerSet& dstPeers)
        {
            if (auto p2pBus = dynamicCast<nx::p2p::MessageBus*>())
                p2pBus->sendTransaction(tran, dstPeers);
            else if (auto msgBus = dynamicCast<QnTransactionMessageBus*>())
                msgBus->sendTransaction(tran, dstPeers);
        }

        template<class T>
        void sendTransaction(
            const QnTransaction<T>& tran,
            const QnUuid& peer)
        {
            if (auto p2pBus = dynamicCast<nx::p2p::MessageBus*>())
                p2pBus->sendTransaction(tran, QnPeerSet() << peer);
            else if (auto msgBus = dynamicCast<QnTransactionMessageBus*>())
                msgBus->sendTransaction(tran, QnPeerSet() << peer);
            else
                NX_CRITICAL(false, "Not implemented");
        }
    private:
        std::unique_ptr<TransactionMessageBusBase> m_bus;

        detail::QnDbManager* m_db;
        Qn::PeerType m_peerType;
        QnJsonTransactionSerializer* m_jsonTranSerializer;
        QnUbjsonTransactionSerializer* m_ubjsonTranSerializer;
        TimeSynchronizationManager* m_timeSyncManager;
    };
};
