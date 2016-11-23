#pragma once

#include <nx/network/buffer.h>
#include <nx/utils/move_only_func.h>
#include <nx/utils/std/cpp14.h>

#include <transaction/transaction.h>
#include <utils/db/types.h>
#include <utils/db/query_context.h>

namespace nx {
namespace cdb {
namespace ec2 {
namespace dao {

struct TransactionData
{
    const nx::String& systemId;
    const ::ec2::QnAbstractTransaction& header;
    const QByteArray& hash;
    const QByteArray& ubjsonSerializedTransaction;
};

class AbstractTransactionDataObject
{
public:
    using FactoryFunc = nx::utils::MoveOnlyFunc<std::unique_ptr<AbstractTransactionDataObject>()>;

    virtual ~AbstractTransactionDataObject() = default;

    virtual nx::db::DBResult insertOrReplaceTransaction(
        nx::db::QueryContext* queryContext,
        const TransactionData& transactionData) = 0;

    static std::unique_ptr<AbstractTransactionDataObject> create();
    
    template<typename CustomDataObjectType>
    static void setDataObjectType()
    {
        setFactoryFunc([](){ return std::make_unique<CustomDataObjectType>(); });
    }

    static void setFactoryFunc(FactoryFunc func);
};

} // namespace dao
} // namespace ec2
} // namespace cdb
} // namespace nx
