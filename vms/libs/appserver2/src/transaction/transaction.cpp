// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "transaction.h"

#include <atomic>

#include <QtCore/QCryptographicHash>

#include "common/common_module.h"
#include "utils/common/synctime.h"

#include "nx/fusion/model_functions.h"
#include "transaction_descriptor.h"

namespace ec2 {

namespace ApiCommand {

QString toString(Value value)
{
    switch (value)
    {
        #define CASE(VALUE, NAME, ...) case NAME: return NX_FMT(#NAME);
        TRANSACTION_DESCRIPTOR_LIST(CASE)
        #undef CASE
        case CompositeSave: return NX_FMT("CompositeSave");
        case NotDefined: return NX_FMT("NotDefined");
    }

    NX_ASSERT(false, "Unknown transaction value %1", (int) value);
    return NX_FMT("Transation_%1", (int) value);
}

Value fromString(const QString& name)
{
    auto descriptor = getTransactionDescriptorByName(name);
    return descriptor ? descriptor->getValue() : NotDefined;
}

bool isSystem(Value value)
{
    switch (value)
    {
        #define CASE(VALUE, NAME, TYPE, PERSISTENT, SYSTEM, ...) case NAME: return SYSTEM;
        TRANSACTION_DESCRIPTOR_LIST(CASE)
        #undef CASE
        case CompositeSave: return false;
        case NotDefined: return false;
    }

    NX_ASSERT(false, "Unknown transaction value %1", (int) value);
    return false;
}

bool isPersistent(Value value)
{
    switch (value)
    {
        #define CASE(VALUE, NAME, TYPE, PERSISTENT, ...) case NAME: return PERSISTENT;
        TRANSACTION_DESCRIPTOR_LIST(CASE)
        #undef CASE
        case CompositeSave: return true;
        case NotDefined: return false;
    }

    NX_ASSERT(false, "Unknown transaction value %1", (int) value);
    return false;
}

} // namespace ApiCommand

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

QN_FUSION_ADAPT_STRUCT_FUNCTIONS(TransactionModel, (json), TransactionModel_Fields)

void TransactionModel::setError(
    const QString& message, const QByteArray& binaryData, bool withData)
{
    _error = message;
    QnUbjsonReader<QByteArray> stream(&binaryData);
    if (QJsonValue value; QnUbjson::deserialize(&stream, &value))
        data = value;
    else
        data = QnLexical::serialized(binaryData);
    NX_WARNING(NX_SCOPE_TAG, "%1", QJson::serialized(*this));
    if (!withData)
        data.reset();
}

} // namespace ec2
