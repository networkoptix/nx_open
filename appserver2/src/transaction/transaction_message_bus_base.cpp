#include "transaction_message_bus_base.h"
#include <common/common_module.h>

namespace ec2 {

QnTransactionMessageBusBase::QnTransactionMessageBusBase(
    detail::QnDbManager* db,
    Qn::PeerType peerType,
    QnCommonModule* commonModule)
:
    QnCommonModuleAware(commonModule),
    m_db(db),
    m_localPeerType(peerType),
    m_mutex(QnMutex::Recursive)
{
}

} // namespace ec2
