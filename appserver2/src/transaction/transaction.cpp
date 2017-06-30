#include "transaction.h"

#include <atomic>

#include "common/common_module.h"
#include "database/db_manager.h"
#include "utils/common/synctime.h"

#include "nx/fusion/model_functions.h"
#include "transaction_descriptor.h"

namespace ec2 {

namespace ApiCommand
{
    QString toString(Value val) { return getTransactionDescriptorByValue(val)->getName(); }

    Value fromString(const QString& val)
	{
        auto descriptor = getTransactionDescriptorByName(val);
        return descriptor ? descriptor->getValue() : ApiCommand::NotDefined;
    }

    bool isSystem(Value val) { return getTransactionDescriptorByValue(val)->isSystem; }

    bool isPersistent(Value val) { return getTransactionDescriptorByValue(val)->isPersistent; }
}

int generateRequestID()
{
    static std::atomic<int> requestId(0);
    return ++requestId;
}

QString QnAbstractTransaction::toString() const
{
    return lm("command=%1 time=%2 peer=%3 dbId=%4 dbSeq=%5")
        .arg(ApiCommand::toString(command))
        .arg(persistentInfo.timestamp)
        .arg(peerID.toString())
        .arg(persistentInfo.dbID.toString())
        .arg(persistentInfo.sequence);
}

QN_FUSION_ADAPT_STRUCT_FUNCTIONS(
    QnAbstractTransaction::PersistentInfo,
    (json)(ubjson)(xml)(csv_record),
    QnAbstractTransaction_PERSISTENT_Fields,
    (optional, true))

QN_FUSION_ADAPT_STRUCT_FUNCTIONS_FOR_TYPES(
    (HistoryAttributes)(QnAbstractTransaction)(ApiTransactionData),
    (json)(ubjson)(xml)(csv_record),
    _Fields,
    (optional, true))

} // namespace ec2
