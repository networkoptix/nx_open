#pragma once

#include <map>

#include <boost/multi_index_container.hpp>
#include <boost/multi_index/identity.hpp>
#include <boost/multi_index/member.hpp>
#include <boost/multi_index/ordered_index.hpp>

#include <nx/utils/thread/mutex.h>

#include <nx/sql/types.h>
#include <nx/sql/query_context.h>

#include "../abstract_transaction_data_object.h"

namespace nx::clusterdb::engine::dao::memory {

class NX_DATA_SYNC_ENGINE_API TransactionDataObject:
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
    struct TransactionSourceKey
    {
        std::string systemId;
        QString peerId;
        QString dbInstanceId;

        bool operator<(const TransactionSourceKey& rhs) const;
    };

    struct TransactionKey
    {
        TransactionSourceKey sourceKey;
        std::int64_t sequence;

        bool operator<(const TransactionKey& rhs) const;
        bool operator<=(const TransactionKey& rhs) const;
    };

    struct Transaction
    {
        TransactionKey uniqueKey;
        QByteArray hash;
        QByteArray ubjsonSerializedTransaction;
    };

    typedef boost::multi_index::multi_index_container<
        Transaction,
        boost::multi_index::indexed_by<
            boost::multi_index::ordered_unique<boost::multi_index::member<
                Transaction, TransactionKey, &Transaction::uniqueKey>>,
            boost::multi_index::ordered_unique<boost::multi_index::member<
                Transaction, QByteArray, &Transaction::hash>>
            >
        > TransactionDictionary;

    constexpr static const int kIndexBySourceAndSequence = 0;
    constexpr static const int kIndexByHash = 1;

    QnMutex m_mutex;
    TransactionDictionary m_transactions;
    const int m_transactionFormatVersion;
    std::map<std::string /*peerId*/,
        std::map<std::string /*systemId*/, int /*sequence*/>> m_recentSequences;
};

} // namespace nx::clusterdb::engine::dao::memory
