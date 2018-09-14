#include "transaction.h"

#include <atomic>

#include "common/common_module.h"
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

QnUuid QnAbstractTransaction::makeHash(const QByteArray& data1, const QByteArray& data2)
{
    QCryptographicHash hash(QCryptographicHash::Md5);
    hash.addData(data1);
    if (!data2.isEmpty())
        hash.addData(data2);
    return QnUuid::fromRfc4122(hash.result());
}

QnUuid QnAbstractTransaction::makeHash(
    const QByteArray& extraData, const nx::vms::api::DiscoveryData& data)
{
    QCryptographicHash hash(QCryptographicHash::Md5);
    hash.addData(extraData);
    hash.addData(data.url.toUtf8());
    hash.addData(data.id.toString().toUtf8());
    return QnUuid::fromRfc4122(hash.result());
}

} // namespace ec2

#define TRANSACTION_ENUM_APPLY(VALUE, NAME, ...) (ec2::ApiCommand::NAME, #NAME)
QN_DEFINE_EXPLICIT_ENUM_LEXICAL_FUNCTIONS(ec2::ApiCommand, Value,
    NX_MSVC_EXPAND(TRANSACTION_DESCRIPTOR_LIST(TRANSACTION_ENUM_APPLY))
)
#undef TRANSACTION_ENUM_APPLY

QN_DEFINE_EXPLICIT_ENUM_LEXICAL_FUNCTIONS(ec2::TransactionType, Value,
    (ec2::TransactionType::Unknown, "Unknown")
    (ec2::TransactionType::Regular, "Regular")
    (ec2::TransactionType::Local, "Local")
    (ec2::TransactionType::Cloud, "Cloud")
)
