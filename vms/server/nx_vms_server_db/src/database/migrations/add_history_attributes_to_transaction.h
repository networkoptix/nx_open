#pragma once

#include <QtSql/QSqlDatabase>

#include "make_transaction_timestamp_128bit.h"
#include "transaction/transaction.h"

namespace ec2 {
namespace migration {
namespace add_history {

namespace before {

typedef timestamp128bit::after::QnAbstractTransaction QnAbstractTransaction;

} // namespace before

namespace after {

struct HistoryAttributes
{
    /** Id of user or entity who created transaction. */
    QnUuid author;
};

QN_FUSION_DECLARE_FUNCTIONS(HistoryAttributes, (ubjson))

class QnAbstractTransaction:
    public before::QnAbstractTransaction
{
public:
    HistoryAttributes historyAttributes;

    QnAbstractTransaction& operator=(const before::QnAbstractTransaction& right);
};

QN_FUSION_DECLARE_FUNCTIONS(QnAbstractTransaction, (ubjson))

} // namespace after

bool migrate(QSqlDatabase* const sdb);

} // namespace add_history
} // namespace migration
} // namespace ec2
