// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "ubjson_transaction_serializer.h"

namespace ec2
{
    size_t qHash(const QnUbjsonTransactionSerializer::CacheKey& id)
    {
        QByteArray idData(id.persistentInfo.dbID.toRfc4122());
        idData
            .append((const char*)&id.persistentInfo.timestamp, sizeof(id.persistentInfo.timestamp))
            .append((const char*)&id.persistentInfo.sequence, sizeof(id.persistentInfo.sequence));

        return ::qHash(idData, id.command);
    }
}
