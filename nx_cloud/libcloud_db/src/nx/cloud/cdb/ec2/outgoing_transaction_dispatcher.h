#pragma once

#include <nx/network/buffer.h>
#include <nx/utils/move_only_func.h>

#include <transaction/transaction.h>
#include <nx/utils/subscription.h>

#include "serialization/serializable_transaction.h"
#include "serialization/transaction_serializer.h"
#include "transaction_transport_header.h"

namespace nx {
namespace cdb {
namespace ec2 {

class AbstractOutgoingTransactionDispatcher
{
public:
    virtual ~AbstractOutgoingTransactionDispatcher() = default;

    virtual void dispatchTransaction(
        const nx::String& systemId,
        std::shared_ptr<const SerializableAbstractTransaction> transactionSerializer) = 0;
};

/**
 * Dispatches transactions that has to be sent to other peers.
 */
class OutgoingTransactionDispatcher:
    public AbstractOutgoingTransactionDispatcher
{
public:
    typedef nx::utils::Subscription<
        const nx::String&,
        std::shared_ptr<const SerializableAbstractTransaction>> OnNewTransactionSubscription;
    typedef OnNewTransactionSubscription::NotificationCallback OnNewTransactionHandler;

    OutgoingTransactionDispatcher();

    virtual void dispatchTransaction(
        const nx::String& systemId,
        std::shared_ptr<const SerializableAbstractTransaction> transactionSerializer) override;

    OnNewTransactionSubscription* onNewTransactionSubscription();

private:
    OnNewTransactionSubscription m_onNewTransactionSubscription;
};

} // namespace ec2
} // namespace cdb
} // namespace nx
