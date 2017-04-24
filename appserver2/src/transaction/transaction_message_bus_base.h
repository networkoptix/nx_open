#pragma once

#include <nx_ec/data/api_peer_data.h>
#include <nx/utils/thread/mutex.h>
#include <common/common_module_aware.h>
#include <QtCore/QThread>

namespace ec2
{
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
            QnCommonModule* commonModule);

        struct RoutingRecord
        {
            RoutingRecord(): distance(0), lastRecvTime(0) {}
            RoutingRecord(int distance, qint64 lastRecvTime): distance(distance), lastRecvTime(lastRecvTime) {}

            quint32 distance;
            qint64 lastRecvTime;
        };
        virtual ~QnTransactionMessageBusBase();

        virtual void start();
        virtual void stop();

        void setHandler(ECConnectionNotificationManager* handler);
        void removeHandler(ECConnectionNotificationManager* handler);

    protected:
        detail::QnDbManager* m_db = nullptr;
        QThread* m_thread = nullptr;
        ECConnectionNotificationManager* m_handler = nullptr;

        /** Info about us. */
        Qn::PeerType m_localPeerType = Qn::PT_NotDefined;

        mutable QnMutex m_mutex;
    };
};
