#pragma once

#include <nx/sql/types.h>
#include <nx/sql/query_context.h>

#include "../abstract_transaction_data_object.h"

namespace nx::clusterdb::engine::dao::rdb {

class TransactionDataObject:
    public AbstractTransactionDataObject
{
public:
    TransactionDataObject(int transactionFormatVersion);

    virtual nx::sql::DBResult insertOrReplaceTransaction(
        nx::sql::QueryContext* queryContext,
        const TransactionData& transactionData) override;

    virtual nx::sql::DBResult updateTimestampHiForSystem(
        nx::sql::QueryContext* queryContext,
        const std::string& systemId,
        quint64 newValue) override;

    virtual nx::sql::DBResult fetchTransactionsOfAPeerQuery(
        nx::sql::QueryContext* queryContext,
        const std::string& systemId,
        const QString& peerId,
        const QString& dbInstanceId,
        std::int64_t minSequence,
        std::int64_t maxSequence,
        std::vector<dao::TransactionLogRecord>* const transactions) override;

    virtual void saveRecentTransactionSequence(
        nx::sql::QueryContext* queryContext,
        const std::string& systemId,
        const std::string& peerId,
        int sequence) override;

    virtual std::map<std::string /*systemId*/, int /*sequence*/> fetchRecentTransactionSequence(
        nx::sql::QueryContext* queryContext,
        const std::string& peerId) override;

private:
    const int m_transactionFormatVersion;
};

} // namespace nx::clusterdb::engine::dao::rdb
