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
};

} // namespace rdb
} // namespace dao
} // namespace ec2
} // namespace cdb
} // namespace nx
