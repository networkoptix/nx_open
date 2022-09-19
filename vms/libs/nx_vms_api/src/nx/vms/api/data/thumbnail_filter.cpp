// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "thumbnail_filter.h"

#include <nx/fusion/model_functions.h>

namespace nx::vms::api {

QN_FUSION_ADAPT_STRUCT_FUNCTIONS(ThumbnailFilter, (json), ThumbnailFilter_Fields)

bool deserialize(QnJsonContext* /*ctx*/, const QJsonValue& value, ThumbnailSize* target)
{
    if (value.isString())
    {
        std::regex re("^([+-]?\\d+)x([+-]?\\d+)$");
        std::smatch match;

        const auto str = value.toString().toStdString();
        if (!std::regex_search(str, match, re))
            return false;

        try
        {
            target->setWidth(std::stoi(match[1]));
            target->setHeight(std::stoi(match[2]));
        }
        catch (const std::exception& e)
        {
            NX_DEBUG(NX_SCOPE_TAG, "Failed to deserialize ThumbnailSize: %1", e.what());
            return false;
        }
        return true;
    }
    return false;
}

bool deserialize(QnJsonContext* /*ctx*/, const QJsonValue& value, ThumbnailCrop* target)
{
    return value.isString() && QnLexical::deserialize<QRectF>(value.toString(), target);
}

} // namespace nx::vms::api
