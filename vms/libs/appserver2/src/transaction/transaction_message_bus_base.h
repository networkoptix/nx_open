// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <QtCore/QThread>

#include <core/resource_access/user_access_data.h>
#include <nx/vms/api/types/connection_types.h>

#include "abstract_transaction_message_bus.h"

namespace ec2
{
    class TransactionMessageBusBase: public AbstractTransactionMessageBus
    {
    public:
        TransactionMessageBusBase(
            nx::vms::api::PeerType peerType,
            nx::vms::common::SystemContext* systemContext,
            QnJsonTransactionSerializer* jsonTranSerializer,
            QnUbjsonTransactionSerializer* ubjsonTranSerializer
        );

        struct RoutingRecord
        {
            RoutingRecord(): distance(0), lastRecvTime(0) {}
            RoutingRecord(int distance, qint64 lastRecvTime): distance(distance), lastRecvTime(lastRecvTime) {}

            qint32 distance;
            qint64 lastRecvTime;
        };
        virtual ~TransactionMessageBusBase();

        virtual void start() override;
        virtual void stop() override;

        virtual void setHandler(ECConnectionNotificationManager* handler) override;
        virtual void removeHandler(ECConnectionNotificationManager* handler) override;

        virtual QnJsonTransactionSerializer* jsonTranSerializer() const override;
        virtual QnUbjsonTransactionSerializer* ubjsonTranSerializer() const override;

        virtual ConnectionGuardSharedState* connectionGuardSharedState() override;

    protected:
        QThread* m_thread = nullptr;
        ECConnectionNotificationManager* m_handler = nullptr;

        QnJsonTransactionSerializer* m_jsonTranSerializer = nullptr;
        QnUbjsonTransactionSerializer* m_ubjsonTranSerializer = nullptr;

        /** Info about us. */
        nx::vms::api::PeerType m_localPeerType = nx::vms::api::PeerType::notDefined;

        mutable nx::Mutex m_mutex;
        ConnectionGuardSharedState m_connectionGuardSharedState;
    };
};
