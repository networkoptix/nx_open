#pragma once

#include <nx/network/buffer.h>
#include <nx/utils/move_only_func.h>
#include <nx/utils/subscription.h>

#include "outgoing_command_filter.h"
#include "serialization/serializable_transaction.h"
#include "serialization/transaction_serializer.h"
#include "transaction_transport_header.h"

namespace nx::clusterdb::engine {

class NX_DATA_SYNC_ENGINE_API AbstractOutgoingCommandDispatcher
{
public:
    virtual ~AbstractOutgoingCommandDispatcher() = default;

    virtual void dispatchTransaction(
        const std::string& systemId,
        std::shared_ptr<const SerializableAbstractCommand> transactionSerializer) = 0;
};

/**
 * Dispatches transactions that has to be sent to other peers.
 */
class NX_DATA_SYNC_ENGINE_API OutgoingCommandDispatcher:
    public AbstractOutgoingCommandDispatcher
{
public:
    typedef nx::utils::Subscription<
        const std::string&,
        std::shared_ptr<const SerializableAbstractCommand>> OnNewTransactionSubscription;

    OutgoingCommandDispatcher(
        const OutgoingCommandFilter& filter);

    virtual void dispatchTransaction(
        const std::string& systemId,
        std::shared_ptr<const SerializableAbstractCommand> transactionSerializer) override;

    OnNewTransactionSubscription& onNewTransactionSubscription();

private:
    OnNewTransactionSubscription m_onNewTransactionSubscription;
    const OutgoingCommandFilter& m_filter;
};

} // namespace nx::clusterdb::engine
