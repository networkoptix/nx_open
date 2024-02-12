// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <nx/utils/uuid.h>

NX_VMS_COMMON_API nx::Uuid intToGuid(qint32 value, const QByteArray& postfix);

NX_VMS_COMMON_API QString guidToSqlString(const nx::Uuid& guid);

inline nx::Uuid guidFromArbitraryData(const QByteArray& data)
{
    return nx::Uuid::fromArbitraryData(data);
}

inline nx::Uuid guidFromArbitraryData(const QString& data)
{
    return nx::Uuid::fromArbitraryData(data);
}

inline nx::Uuid guidFromArbitraryData(const std::string& data)
{
    return nx::Uuid::fromArbitraryData(data);
}
