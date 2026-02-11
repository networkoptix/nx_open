// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "int64_as_number.h"

#include <QtCore/QJsonValue>

namespace nx::json {

void serialize(QnJsonContext*, const Int64AsNumber& value, QJsonValue* out)
{
    *out = QJsonValue((double) value.value);
}

bool deserialize(QnJsonContext*, const QJsonValue& value, Int64AsNumber* out)
{
    if (!value.isDouble())
        return false;

    const double doubleValue = value.toDouble();
    const int64_t intValue = (int64_t) doubleValue;
    if ((double) intValue != doubleValue)
        return false;

    out->value = intValue;
    return true;
}

} // namespace nx::json
