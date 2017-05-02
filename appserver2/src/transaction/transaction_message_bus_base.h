#pragma once

#include <QtCore/QThread>

#include <nx_ec/data/api_peer_data.h>
#include <nx/utils/thread/mutex.h>
#include <common/common_module_aware.h>
#include "connection_guard_shared_state.h"

namespace ec2
{
    class QnJsonTransactionSerializer;
    class QnUbjsonTransactionSerializer;

    class ECConnectionNotificationManager;
    namespace detail {
        class QnDbManager;
    }

    class QnTransactionMessageBusBase: public QObject, public QnCommonModuleAware
    {
    public:
        QnTransactionMessageBusBase(
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
        virtual ~QnTransactionMessageBusBase();

        virtual void start();
        virtual void stop();

        void setHandler(ECConnectionNotificationManager* handler);
        void removeHandler(ECConnectionNotificationManager* handler);

        QnJsonTransactionSerializer* jsonTranSerializer() const;
        QnUbjsonTransactionSerializer* ubjsonTranSerializer() const;

        ConnectionGuardSharedState* connectionGuardSharedState();

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

    };
};
