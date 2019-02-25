#pragma once

#include "../transport/transaction_transport_header.h"

namespace nx::clusterdb::engine {

/**
 * Holds transaction inside and is able to serialize it to a requested format.
 * Can add transport header optionally.
 */
class CommandSerializer
{
public:
    virtual ~CommandSerializer() = default;

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
        const CommandTransportHeader& transportHeader,
        int transactionFormatVersion) const = 0;
};

} // namespace nx::clusterdb::engine
