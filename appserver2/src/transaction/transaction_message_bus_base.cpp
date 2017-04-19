#include "transaction_message_bus_base.h"
#include <common/common_module.h>

namespace ec2 {

QnTransactionMessageBusBase::QnTransactionMessageBusBase(QnCommonModule* commonModule):
    QnCommonModuleAware(commonModule),
    m_mutex(QnMutex::Recursive)
{
}

} // namespace ec2
