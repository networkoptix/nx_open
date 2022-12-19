// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "thumbnail_filter.h"

#include <nx/fusion/model_functions.h>

namespace nx::vms::api {

QN_FUSION_ADAPT_STRUCT_FUNCTIONS(ThumbnailFilter, (json), ThumbnailFilter_Fields)

bool deserialize(QnJsonContext* /*ctx*/, const QJsonValue& value, ThumbnailCrop* target)
{
    return value.isString() && QnLexical::deserialize<QRectF>(value.toString(), target);
}

} // namespace nx::vms::api
