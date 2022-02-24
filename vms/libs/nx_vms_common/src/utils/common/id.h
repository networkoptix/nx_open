// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <nx/utils/uuid.h>

NX_VMS_COMMON_API QnUuid intToGuid(qint32 value, const QByteArray& postfix);

NX_VMS_COMMON_API QString guidToSqlString(const QnUuid& guid);

inline QnUuid guidFromArbitraryData(const QByteArray& data)
{
    return QnUuid::fromArbitraryData(data);
}

inline QnUuid guidFromArbitraryData(const QString& data)
{
    return QnUuid::fromArbitraryData(data);
}

inline QnUuid guidFromArbitraryData(const std::string& data)
{
    return QnUuid::fromArbitraryData(data);
}
