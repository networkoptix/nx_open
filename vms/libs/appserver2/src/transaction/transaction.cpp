// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "transaction.h"

#include <atomic>

#include <QtCore/QCryptographicHash>

#include "common/common_module.h"
#include "utils/common/synctime.h"

#include "nx/fusion/model_functions.h"
#include "transaction_descriptor.h"

namespace ec2 {

namespace ApiCommand
{
    QString toString(Value val)
    {
        auto it = detail::transactionDescriptors.get<0>().find(val);
        if (it == detail::transactionDescriptors.get<0>().end())
            return QString::number((int) val);
        return it->get()->getName();
    }

    Value fromString(const QString& val)
    {
        auto descriptor = getTransactionDescriptorByName(val);
        return descriptor ? descriptor->getValue() : ApiCommand::NotDefined;
    }

    bool isSystem(Value val) { return getTransactionDescriptorByValue(val)->isSystem; }
    bool isRemoveOperation(Value val) { return getTransactionDescriptorByValue(val)->isRemoveOperation; }

    bool isPersistent(Value val)
    {
        return val == CompositeSave || getTransactionDescriptorByValue(val)->isPersistent;
    }
}

int generateRequestID()
{
    static std::atomic<int> requestId(0);
    return ++requestId;
}

QString QnAbstractTransaction::toString() const
{
    return nx::format("command=%1 time=%2 peer=%3 dbId=%4 dbSeq=%5")
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

QN_FUSION_ADAPT_STRUCT_FUNCTIONS(
    HistoryAttributes, (json)(ubjson)(xml)(csv_record), HistoryAttributes_Fields, (optional, true))
QN_FUSION_ADAPT_STRUCT_FUNCTIONS(QnAbstractTransaction,
    (json)(ubjson)(xml)(csv_record), QnAbstractTransaction_Fields, (optional, true))
QN_FUSION_ADAPT_STRUCT_FUNCTIONS(ApiTransactionData,
    (json)(ubjson)(xml)(csv_record), ApiTransactionData_Fields, (optional, true))

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

QN_FUSION_ADAPT_STRUCT_FUNCTIONS(ApiTranLogFilter,
    (json)(ubjson)(xml)(csv_record), ApiTranLogFilter_Fields)

} // namespace ec2
