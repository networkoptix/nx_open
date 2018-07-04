#pragma once

#include <nx/sql/types.h>
#include <nx/sql/query_context.h>

#include "../abstract_transaction_data_object.h"

namespace nx {
namespace data_sync_engine {
namespace dao {
namespace rdb {

class TransactionDataObject:
    public AbstractTransactionDataObject
{
public:
    virtual nx::sql::DBResult insertOrReplaceTransaction(
        nx::sql::QueryContext* queryContext,
        const TransactionData& transactionData) override;

    virtual nx::sql::DBResult updateTimestampHiForSystem(
        nx::sql::QueryContext* queryContext,
        const nx::String& systemId,
        quint64 newValue) override;

    virtual nx::sql::DBResult fetchTransactionsOfAPeerQuery(
        nx::sql::QueryContext* queryContext,
        const nx::String& systemId,
        const QString& peerId,
        const QString& dbInstanceId,
        std::int64_t minSequence,
        std::int64_t maxSequence,
        std::vector<dao::TransactionLogRecord>* const transactions) override;
};

} // namespace rdb
} // namespace dao
} // namespace data_sync_engine
} // namespace nx
