#pragma once

#include <QtSql/QSqlDatabase>

#include <nx/fusion/model_functions_fwd.h>
#include <nx/utils/uuid.h>

#include "add_transaction_type.h"
#include "transaction/transaction.h"

namespace ec2 {
namespace migration {
namespace timestamp128bit {

namespace before {

typedef addTransactionType::after::QnAbstractTransaction QnAbstractTransaction;

} // namespace before

namespace after {

class Timestamp
{
public:
    quint64 sequence;
    quint64 ticks;

    Timestamp():
        sequence(0),
        ticks(0)
    {
    }

    static Timestamp fromInteger(long long value)
    {
        return Timestamp(value);
    }

private:
    Timestamp(long long value):
        sequence(0),
        ticks(static_cast<quint64>(value))
    {
    }
};

QN_FUSION_DECLARE_FUNCTIONS(Timestamp, (ubjson))

struct PersistentInfo
{
    typedef Timestamp TimestampType;

    PersistentInfo():
        sequence(0)
    {
        timestamp.sequence = 0;
        timestamp.ticks = 0;
    }

    QnUuid dbID;
    qint32 sequence;
    Timestamp timestamp;
};

QN_FUSION_DECLARE_FUNCTIONS(PersistentInfo, (ubjson))


class QnAbstractTransaction
{
public:
    typedef PersistentInfo PersistentInfoType;

    ApiCommand::Value command;
    QnUuid peerID;
    PersistentInfo persistentInfo;
    TransactionType::Value transactionType;

    QnAbstractTransaction();

    QnAbstractTransaction& operator=(const before::QnAbstractTransaction& right);
};

QN_FUSION_DECLARE_FUNCTIONS(QnAbstractTransaction, (ubjson))

} // namespace after

bool migrate(QSqlDatabase* const sdb);

} // namespace add_history
} // namespace migration
} // namespace ec2
