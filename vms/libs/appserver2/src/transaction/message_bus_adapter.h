#pragma once

#include <QtCore/QThread>

#include "abstract_transaction_message_bus.h"
#include <transaction/transaction_message_bus.h>
#include <nx/p2p/p2p_message_bus.h>

namespace ec2 {

    class TransactionMessageBusAdapter:
        public AbstractTransactionMessageBus
    {
        Q_OBJECT

    public:
        TransactionMessageBusAdapter(
            QnCommonModule* commonModule,
            QnJsonTransactionSerializer* jsonTranSerializer,
            QnUbjsonTransactionSerializer* ubjsonTranSerializer
        );

        template <typename MessageBusType>
        MessageBusType* init(nx::vms::api::PeerType peerType)
        {
            reset();
            m_bus.reset(new MessageBusType(
                peerType,
                commonModule(),
                m_jsonTranSerializer,
                m_ubjsonTranSerializer));

            initInternal();
            return dynamic_cast<MessageBusType*> (m_bus.get());
        }

        void reset();

        template <typename T> T dynamicCast() { return dynamic_cast<T> (m_bus.get()); }

        virtual ~TransactionMessageBusAdapter() = default;

        virtual void start() override;
        virtual void stop() override;

        virtual QSet<QnUuid> directlyConnectedClientPeers() const override;
        virtual QSet<QnUuid> directlyConnectedServerPeers() const override;

        virtual QnUuid routeToPeerVia(
            const QnUuid& dstPeer, int* distance, nx::network::SocketAddress* knownPeerAddress) const override;
        virtual int distanceToPeer(const QnUuid& dstPeer) const override;

        virtual void addOutgoingConnectionToPeer(
            const QnUuid& id,
            nx::vms::api::PeerType peerType,
            const nx::utils::Url& url) override;
        virtual void removeOutgoingConnectionFromPeer(const QnUuid& id) override;

        virtual void dropConnections() override;

        virtual QVector<QnTransportConnectionInfo> connectionsInfo() const override;

        virtual void setHandler(ECConnectionNotificationManager* handler) override;
        virtual void removeHandler(ECConnectionNotificationManager* handler) override;

        virtual QnJsonTransactionSerializer* jsonTranSerializer() const override;
        virtual QnUbjsonTransactionSerializer* ubjsonTranSerializer() const override;

        virtual ConnectionGuardSharedState* connectionGuardSharedState() override;
        //detail::QnDbManager* getDb() const;

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
            const nx::vms::api::PeerSet& dstPeers)
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
                p2pBus->sendTransaction(tran, {peer});
            else if (auto msgBus = dynamicCast<QnTransactionMessageBus*>())
                msgBus->sendTransaction(tran, {peer});
            else
                NX_CRITICAL(false, "Not implemented");
        }

    private:
        void initInternal();

    private:
        std::unique_ptr<AbstractTransactionMessageBus> m_bus;

        QnJsonTransactionSerializer* m_jsonTranSerializer;
        QnUbjsonTransactionSerializer* m_ubjsonTranSerializer;
    };

} // namespace ec2
