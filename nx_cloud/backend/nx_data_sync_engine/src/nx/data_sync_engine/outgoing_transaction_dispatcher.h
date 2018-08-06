#pragma once

#include <nx/network/buffer.h>
#include <nx/utils/move_only_func.h>
#include <nx/utils/subscription.h>

#include "serialization/serializable_transaction.h"
#include "serialization/transaction_serializer.h"
#include "transaction_transport_header.h"

namespace nx {
namespace data_sync_engine {

class NX_DATA_SYNC_ENGINE_API AbstractOutgoingTransactionDispatcher
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
class NX_DATA_SYNC_ENGINE_API OutgoingTransactionDispatcher:
    public AbstractOutgoingTransactionDispatcher
{
public:
    typedef nx::utils::Subscription<
        const nx::String&,
        std::shared_ptr<const SerializableAbstractTransaction>> OnNewTransactionSubscription;

    OutgoingTransactionDispatcher();

    virtual void dispatchTransaction(
        const nx::String& systemId,
        std::shared_ptr<const SerializableAbstractTransaction> transactionSerializer) override;

    OnNewTransactionSubscription* onNewTransactionSubscription();

private:
    OnNewTransactionSubscription m_onNewTransactionSubscription;
};

} // namespace data_sync_engine
} // namespace nx
