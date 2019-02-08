#pragma once

#include <nx/network/buffer.h>
#include <nx/utils/basic_factory.h>
#include <nx/utils/move_only_func.h>
#include <nx/utils/std/cpp14.h>

#include <nx/sql/types.h>
#include <nx/sql/query_context.h>

#include "../serialization/ubjson_serialized_transaction.h"

namespace nx::clusterdb::engine::dao {

struct TransactionData
{
    const std::string& systemId;
    const CommandHeader& header;
    const QByteArray& hash;
    const QByteArray& ubjsonSerializedTransaction;
};

struct TransactionLogRecord
{
    nx::Buffer hash;
    std::unique_ptr<const CommandSerializer> serializer;
};

/**
 * Implementation MUST keep following unique indexes on data:
 * - {system_id, peer_guid, db_guid, sequence}
 * - {system_id, tran_hash}
 * If unique indexes has been violated, transaction gets silently replaced.
 */
class NX_DATA_SYNC_ENGINE_API AbstractTransactionDataObject
{
public:
    virtual ~AbstractTransactionDataObject() = default;

    virtual nx::sql::DBResult insertOrReplaceTransaction(
        nx::sql::QueryContext* queryContext,
        const TransactionData& transactionData) = 0;

    virtual nx::sql::DBResult updateTimestampHiForSystem(
        nx::sql::QueryContext* queryContext,
        const std::string& systemId,
        quint64 newValue) = 0;

    // TODO: #ak Too many arguments in following method.
    /**
     * @param minSequence This value is non-inclusive (e.g., the sequence the peer already has).
     * @param maxSequence This value is inclusive
     * (e.g., max sequence known in the cluster and, thus, desirable by the peer).
     */
    virtual nx::sql::DBResult fetchTransactionsOfAPeerQuery(
        nx::sql::QueryContext* queryContext,
        const std::string& systemId,
        const QString& peerId,
        const QString& dbInstanceId,
        std::int64_t minSequence,
        std::int64_t maxSequence,
        std::vector<dao::TransactionLogRecord>* const transactions) = 0;
};

//-------------------------------------------------------------------------------------------------

enum class DataObjectType
{
    rdbms,
    ram
};

using TransactionDataObjectFactoryFunction =
    std::unique_ptr<AbstractTransactionDataObject>(int /*commandFormatVersion*/);

class NX_DATA_SYNC_ENGINE_API TransactionDataObjectFactory:
    public nx::utils::BasicFactory<TransactionDataObjectFactoryFunction>
{
    using base_type = nx::utils::BasicFactory<TransactionDataObjectFactoryFunction>;

public:
    TransactionDataObjectFactory();

    static TransactionDataObjectFactory& instance();

    template<typename CustomDataObjectType>
    Function setDataObjectType()
    {
        return setCustomFunc(
            [](int commandFormatVersion)
            {
                return std::make_unique<CustomDataObjectType>(commandFormatVersion);
            });
    }

    Function setDataObjectType(DataObjectType dataObjectType);

private:
    std::unique_ptr<AbstractTransactionDataObject> defaultFactoryFunction(
        int commandFormatVersion);
};

} // namespace nx::clusterdb::engine::dao
