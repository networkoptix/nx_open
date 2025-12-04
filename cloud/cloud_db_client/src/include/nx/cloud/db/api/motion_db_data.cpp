// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "motion_db_data.h"

namespace nx::cloud::db::api {

nx::UrlQuery GetMotionParamsData::toUrlQuery() const
{
    nx::UrlQuery result;

    if (!organizationId.empty())
        result.addQueryItem("organizationId", organizationId);

    if (!siteId.empty())
        result.addQueryItem("siteId", siteId);

    for (const auto& device: deviceIds)
        result.addQueryItem("deviceId", device);
    if (startTimeMs)
        result.addQueryItem("startTimeMs", startTimeMs->count());
    if (endTimeMs)
        result.addQueryItem("endTimeMs", endTimeMs->count());
    for (const auto& region: boundingBox)
        result.addQueryItem("boundingBox", region);
    if (limit)
        result.addQueryItem("limit", *limit);
    if (interval)
        result.addQueryItem("interval", interval->count());

    result.addQueryItem("order", toString(order));

    return result;
}

} // namespace nx::cloud::db::api
