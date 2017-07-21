#pragma once

#include <QtCore/QThread>

#include "abstract_transaction_message_bus.h"
#include <core/resource_access/user_access_data.h>

namespace ec2
{
    class TransactionMessageBusBase: public AbstractTransactionMessageBus
    {
        Q_OBJECT
    public:
        TransactionMessageBusBase(
            detail::QnDbManager* db,
            Qn::PeerType peerType,
            QnCommonModule* commonModule,
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
        virtual detail::QnDbManager* getDb() const override { return m_db; }
        virtual void setTimeSyncManager(TimeSynchronizationManager* timeSyncManager) override;

    protected:
        bool readApiFullInfoData(
            const Qn::UserAccessData& userAccess,
            const ec2::ApiPeerData& remotePeer,
            ApiFullInfoData* outData);
    protected:
        detail::QnDbManager* m_db = nullptr;
        QThread* m_thread = nullptr;
        ECConnectionNotificationManager* m_handler = nullptr;

        QnJsonTransactionSerializer* m_jsonTranSerializer = nullptr;
        QnUbjsonTransactionSerializer* m_ubjsonTranSerializer = nullptr;

        /** Info about us. */
        Qn::PeerType m_localPeerType = Qn::PT_NotDefined;

        mutable QnMutex m_mutex;
        ConnectionGuardSharedState m_connectionGuardSharedState;
        TimeSynchronizationManager* m_timeSyncManager = nullptr;
    };
};
