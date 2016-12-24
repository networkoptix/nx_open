#pragma once

#include <utils/db/types.h>
#include <utils/db/query_context.h>

#include "../abstract_transaction_data_object.h"

namespace nx {
namespace cdb {
namespace ec2 {
namespace dao {
namespace rdb {

class TransactionDataObject:
    public AbstractTransactionDataObject
{
public:
    virtual nx::db::DBResult insertOrReplaceTransaction(
        nx::db::QueryContext* queryContext,
        const TransactionData& transactionData) override;

    virtual nx::db::DBResult updateTimestampHiForSystem(
        nx::db::QueryContext* queryContext,
        const nx::String& systemId,
        quint64 newValue) override;

    virtual nx::db::DBResult fetchTransactionsOfAPeerQuery(
        nx::db::QueryContext* queryContext,
        const nx::String& systemId,
        const QString& peerId,
        const QString& dbInstanceId,
        std::int64_t minSequence,
        std::int64_t maxSequence,
        std::vector<dao::TransactionLogRecord>* const transactions) override;
};

} // namespace rdb
} // namespace dao
} // namespace ec2
} // namespace cdb
} // namespace nx
