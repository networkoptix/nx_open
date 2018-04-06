#pragma once

#include <nx/network/buffer.h>
#include <nx/utils/move_only_func.h>
#include <nx/utils/std/cpp14.h>

#include <transaction/transaction.h>
#include <nx/utils/db/types.h>
#include <nx/utils/db/query_context.h>

#include "../serialization/ubjson_serialized_transaction.h"

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

struct TransactionLogRecord
{
    nx::Buffer hash;
    std::unique_ptr<const TransactionSerializer> serializer;
};

/**
 * Implementation MUST keep following unique indexes on data:
 * - {system_id, peer_guid, db_guid, sequence}
 * - {system_id, tran_hash}
 * If unique indexes has been violated, transaction gets silently replaced.
 */
class AbstractTransactionDataObject
{
public:
    virtual ~AbstractTransactionDataObject() = default;

    virtual nx::utils::db::DBResult insertOrReplaceTransaction(
        nx::utils::db::QueryContext* queryContext,
        const TransactionData& transactionData) = 0;

    virtual nx::utils::db::DBResult updateTimestampHiForSystem(
        nx::utils::db::QueryContext* queryContext,
        const nx::String& systemId,
        quint64 newValue) = 0;

    // TODO: #ak Too many arguments in following method.
    virtual nx::utils::db::DBResult fetchTransactionsOfAPeerQuery(
        nx::utils::db::QueryContext* queryContext,
        const nx::String& systemId,
        const QString& peerId,
        const QString& dbInstanceId,
        std::int64_t minSequence,
        std::int64_t maxSequence,
        std::vector<dao::TransactionLogRecord>* const transactions) = 0;
};

enum class DataObjectType
{
    rdbms,
    ram
};

class TransactionDataObjectFactory
{
public:
    using FactoryFunc = nx::utils::MoveOnlyFunc<std::unique_ptr<AbstractTransactionDataObject>()>;

    static std::unique_ptr<AbstractTransactionDataObject> create();

    template<typename CustomDataObjectType>
    static void setDataObjectType()
    {
        setFactoryFunc([](){ return std::make_unique<CustomDataObjectType>(); });
    }

    static void setDataObjectType(DataObjectType dataObjectType);

    static void setFactoryFunc(FactoryFunc func);

    static void resetToDefaultFactory();
};

} // namespace dao
} // namespace ec2
} // namespace cdb
} // namespace nx
