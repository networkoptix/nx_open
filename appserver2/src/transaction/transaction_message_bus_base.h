#pragma once

#include <nx_ec/data/api_peer_data.h>
#include <nx/utils/thread/mutex.h>
#include <common/common_module_aware.h>

namespace ec2
{
    class QnTransactionMessageBusBase: public QnCommonModuleAware
    {
    public:
        QnTransactionMessageBusBase(QnCommonModule* commonModule);

        struct RoutingRecord
        {
            RoutingRecord(): distance(0), lastRecvTime(0) {}
            RoutingRecord(int distance, qint64 lastRecvTime): distance(distance), lastRecvTime(lastRecvTime) {}

            quint32 distance;
            qint64 lastRecvTime;
        };

    protected:
        mutable QnMutex m_mutex;
    };
};
