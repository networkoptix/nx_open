// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "rect_as_string.h"

#include <nx/fusion/model_functions.h>

namespace nx::vms::api {

bool deserialize(QnJsonContext*, const QJsonValue& value, RectAsString* target)
{
    return value.isString() && QnLexical::deserialize<QRectF>(value.toString(), target);
}

} // namespace nx::vms::api
