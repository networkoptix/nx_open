#pragma once

#include <map>

#include <boost/multi_index_container.hpp>
#include <boost/multi_index/identity.hpp>
#include <boost/multi_index/member.hpp>
#include <boost/multi_index/ordered_index.hpp>

#include <nx/utils/thread/mutex.h>

#include <nx/utils/db/types.h>
#include <nx/utils/db/query_context.h>

#include "../abstract_transaction_data_object.h"

namespace nx {
namespace cdb {
namespace ec2 {
namespace dao {
namespace memory {

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

private:
    struct TransactionSourceKey
    {
        nx::String systemId;
        QString peerId;
        QString dbInstanceId;

        bool operator<(const TransactionSourceKey& rhs) const;
    };

    struct TransactionKey
    {
        TransactionSourceKey sourceKey;
        std::int64_t sequence;

        bool operator<(const TransactionKey& rhs) const;
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
};

} // namespace memory
} // namespace dao
} // namespace ec2
} // namespace cdb
} // namespace nx
