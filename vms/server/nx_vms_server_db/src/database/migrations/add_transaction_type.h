#pragma once

#include <QtSql/QSqlDatabase>

#include <nx/fusion/model_functions_fwd.h>
#include <nx/utils/uuid.h>

#include "legacy_transaction_migration.h"
#include "transaction/transaction.h"

namespace ec2 {
namespace migration {
namespace addTransactionType {

namespace before {

typedef legacy::QnAbstractTransactionV1 QnAbstractTransaction;

} // namespace before

namespace after {

struct QnAbstractTransaction
{
    typedef legacy::PersistentInfoV1 PersistentInfo;

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
