#pragma once

#include "../transaction_transport_header.h"

namespace nx::data_sync_engine {

/**
 * Holds transaction inside and is able to serialize it to a requested format.
 * Can add transport header optionally.
 */
class TransactionSerializer
{
public:
    virtual ~TransactionSerializer() = default;

    /**
     * Serialize transaction into targetFormat.
     * @note Method is re-enterable
     */
    virtual nx::Buffer serialize(
        Qn::SerializationFormat targetFormat,
        int transactionFormatVersion) const = 0;
    /**
     * Serialize transaction into targetFormat while adding transport header.
     * @note Method is re-enterable
     */
    virtual nx::Buffer serialize(
        Qn::SerializationFormat targetFormat,
        const TransactionTransportHeader& transportHeader,
        int transactionFormatVersion) const = 0;
};

} // namespace nx::data_sync_engine
