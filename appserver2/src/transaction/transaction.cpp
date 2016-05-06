#include "transaction.h"

#include <atomic>

#include "common/common_module.h"
#include "database/db_manager.h"
#include "utils/common/synctime.h"

#include "utils/common/model_functions.h"
#include "transaction_descriptor.h"

namespace ec2
{

    namespace ApiCommand
    {
        QString toString(Value val) { return getTransactionDescriptorByValue(val)->getName(); }

        Value fromString(const QString& val) { return getTransactionDescriptorByName(val)->getValue(); }

        bool isSystem( Value val ) { return getTransactionDescriptorByValue(val)->isSystem; }

        bool isPersistent( Value val ) { return getTransactionDescriptorByValue(val)->isPersistent; }
    }

    int generateRequestID()
    {
        static std::atomic<int> requestID( 0 );
        return ++requestID;
    }

    QString QnAbstractTransaction::toString() const
    {
        return lit("command=%1 time=%2 peer=%3 dbId=%4 dbSeq=%5")
            .arg(ApiCommand::toString(command))
            .arg(persistentInfo.timestamp)
            .arg(peerID.toString())
            .arg(persistentInfo.dbID.toString())
            .arg(persistentInfo.sequence);
    }

    QN_FUSION_ADAPT_STRUCT_FUNCTIONS(QnAbstractTransaction::PersistentInfo,    (json)(ubjson)(xml)(csv_record),   QnAbstractTransaction_PERSISTENT_Fields)
    QN_FUSION_ADAPT_STRUCT_FUNCTIONS(QnAbstractTransaction,                    (json)(ubjson)(xml)(csv_record),   QnAbstractTransaction_Fields)
    QN_FUSION_ADAPT_STRUCT_FUNCTIONS(ApiTransactionData,                    (json)(ubjson)(xml)(csv_record),   ApiTransactionDataFields)

} // namespace ec2

