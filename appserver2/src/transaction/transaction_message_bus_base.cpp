#include "transaction_message_bus_base.h"
#include <common/common_module.h>

namespace ec2 {

QnTransactionMessageBusBase::QnTransactionMessageBusBase(QnCommonModule* commonModule):
    QnCommonModuleAware(commonModule),
    m_mutex(QnMutex::Recursive)
{
}

QnUuid QnTransactionMessageBusBase::routeToPeerVia(const QnUuid& dstPeer, int* peerDistance) const
 {
    QnMutexLocker lock(&m_mutex);
    *peerDistance = INT_MAX;
    const auto itr = m_alivePeers.find(dstPeer);
    if (itr == m_alivePeers.cend())
        return QnUuid(); // route info not found
    const AlivePeerInfo& peerInfo = itr.value();
    int minDistance = INT_MAX;
    QnUuid result;
    for (auto itr2 = peerInfo.routingInfo.cbegin(); itr2 != peerInfo.routingInfo.cend(); ++itr2)
    {
        int distance = itr2.value().distance;
        if (distance < minDistance)
        {
            minDistance = distance;
            result = itr2.key();
        }
    }
    *peerDistance = minDistance;
    return result;
}

int QnTransactionMessageBusBase::distanceToPeer(const QnUuid& dstPeer) const
{
    if (dstPeer == commonModule()->moduleGUID())
        return 0;

    int minDistance = INT_MAX;
    for (const RoutingRecord& rec : m_alivePeers.value(dstPeer).routingInfo)
        minDistance = qMin(minDistance, rec.distance);
    return minDistance;
}

} // namespace ec2
