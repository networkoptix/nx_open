// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <QtCore/QThread>

#include <nx/p2p/p2p_message_bus.h>

#include "abstract_transaction_message_bus.h"

namespace ec2 {

    class TransactionMessageBusAdapter:
        public AbstractTransactionMessageBus
    {
    public:
        TransactionMessageBusAdapter(
            nx::vms::common::SystemContext* systemContext,
            QnJsonTransactionSerializer* jsonTranSerializer,
            QnUbjsonTransactionSerializer* ubjsonTranSerializer
        );

        template <typename MessageBusType, typename SystemContextType>
        MessageBusType* init(nx::vms::api::PeerType peerType, SystemContextType* systemContext)
        {
            reset();
            m_bus.reset(new MessageBusType(
                systemContext,
                peerType,
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
            const nx::utils::Url& url,
            std::optional<nx::network::http::Credentials> credentials = std::nullopt,
            nx::network::ssl::AdapterFunc adapterFunc = nx::network::ssl::kDefaultCertificateCheck) override;
        virtual void removeOutgoingConnectionFromPeer(const QnUuid& id) override;
        virtual void updateOutgoingConnection(
            const QnUuid& id, nx::network::http::Credentials credentials) override;

        virtual void dropConnections() override;

        virtual ConnectionInfos connectionInfos() const override;

        virtual void setHandler(ECConnectionNotificationManager* handler) override;
        virtual void removeHandler(ECConnectionNotificationManager* handler) override;

        virtual QnJsonTransactionSerializer* jsonTranSerializer() const override;
        virtual QnUbjsonTransactionSerializer* ubjsonTranSerializer() const override;

        virtual ConnectionGuardSharedState* connectionGuardSharedState() override;

        ec2::AbstractTransactionMessageBus* getBus();

    public:
        template<class T>
        void sendTransaction(
            const QnTransaction<T>& tran)
        {
            if (auto p2pBus = dynamicCast<nx::p2p::MessageBus*>())
                p2pBus->sendTransaction(tran);
        }

        template<class T>
        void sendTransaction(
            const QnTransaction<T>& tran,
            const nx::vms::api::PeerSet& dstPeers)
        {
            if (auto p2pBus = dynamicCast<nx::p2p::MessageBus*>())
                p2pBus->sendTransaction(tran, dstPeers);
        }

        template<class T>
        void sendTransaction(
            const QnTransaction<T>& tran,
            const QnUuid& peer)
        {
            if (auto p2pBus = dynamicCast<nx::p2p::MessageBus*>())
                p2pBus->sendTransaction(tran, {peer});
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
