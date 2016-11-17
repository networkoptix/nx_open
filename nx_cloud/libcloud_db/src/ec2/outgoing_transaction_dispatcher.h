#pragma once

#include <nx/network/buffer.h>
#include <nx/utils/move_only_func.h>

#include <transaction/transaction.h>
#include <utils/common/subscription.h>

#include "serialization/serializable_transaction.h"
#include "serialization/transaction_serializer.h"
#include "transaction_transport_header.h"

namespace nx {
namespace cdb {
namespace ec2 {

/**
 * Dispatches transactions that has to be sent to other peers.
 */
class OutgoingTransactionDispatcher
{
public:
    typedef nx::utils::Subscription<
        const nx::String&,
        std::shared_ptr<const SerializableAbstractTransaction>> OnNewTransactionSubscription;
    typedef OnNewTransactionSubscription::NotificationCallback OnNewTransactionHandler;

    OutgoingTransactionDispatcher();

    void dispatchTransaction(
        const nx::String& systemId,
        std::shared_ptr<const SerializableAbstractTransaction> transactionSerializer);

    OnNewTransactionSubscription* onNewTransactionSubscription();

private:
    OnNewTransactionSubscription m_onNewTransactionSubscription;
};

} // namespace ec2
} // namespace cdb
} // namespace nx
