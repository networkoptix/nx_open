#include "json_transaction_serializer.h"

QByteArray ec2::QnJsonTransactionSerializer::serializedTransactionWithoutHeader(const QByteArray &serializedTran)
{
    return serializedTran;
}

QByteArray ec2::QnJsonTransactionSerializer::serializedTransactionWithHeader(const QByteArray &serializedTran, const QnTransactionTransportHeader &header)
{
    QJsonValue jsonHeader;
    QJson::serialize(header, &jsonHeader);
    const QByteArray& serializedHeader = QJson::serialized(jsonHeader);

    return
        "{\n"
        "\"header\": "+serializedHeader+",\n"
        "\"tran\": "+serializedTran+"\n"
        "}";
}

bool ec2::QnJsonTransactionSerializer::deserializeTran(const quint8* chunkPayload, int len, QnTransactionTransportHeader& transportHeader, QByteArray& tranData)
{
    QByteArray srcData = QByteArray::fromRawData((const char*) chunkPayload, len);
    QJsonObject tranObject;
    if( !QJson::deserialize(srcData, &tranObject) )
        return false;
    if( !QJson::deserialize( tranObject["header"], &transportHeader ) )
        return false;
    tranData = QByteArray( (const char*)chunkPayload, len );
    return true;
}