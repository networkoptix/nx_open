#pragma once

#include <nx/utils/db/types.h>
#include <nx/utils/db/query_context.h>

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
    virtual nx::utils::db::DBResult insertOrReplaceTransaction(
        nx::utils::db::QueryContext* queryContext,
        const TransactionData& transactionData) override;

    virtual nx::utils::db::DBResult updateTimestampHiForSystem(
        nx::utils::db::QueryContext* queryContext,
        const nx::String& systemId,
        quint64 newValue) override;

    virtual nx::utils::db::DBResult fetchTransactionsOfAPeerQuery(
        nx::utils::db::QueryContext* queryContext,
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
