// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "thumbnail_filter.h"

#include <nx/fusion/model_functions.h>

namespace nx::vms::api {

QN_FUSION_ADAPT_STRUCT(ThumbnailFilter, ThumbnailFilter_Fields)

void serialize(QnJsonContext* ctx, const ThumbnailFilter& value, QJsonValue* target)
{
    QnFusion::serialize(ctx, value, target);
}

bool deserialize(QnJsonContext* ctx, const QJsonValue& value, ThumbnailFilter* target)
{
    if (!QnFusion::deserialize(ctx, value, target))
        return false;

    if (target->timestampUs && target->timestampMs != target->timestampUs)
    {
        if (target->timestampMs != ThumbnailFilter::kLatestThumbnail)
        {
            ctx->setFailedKeyValue(std::pair<QString, QString>{
                "timestampUs", "Must not contradict specified `timestampMs` value"});
            return false;
        }
        target->timestampMs = std::chrono::duration_cast<std::chrono::milliseconds>(
            *target->timestampUs);
    }
    return true;
}

} // namespace nx::vms::api
