// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "analytics_db_data.h"

namespace nx::cloud::db::api {

nx::UrlQuery GetTracksParamsData::toUrlQuery() const
{
    nx::UrlQuery query;

    for (const auto& id: deviceIds)
        query.addQueryItem("deviceId", id.toSimpleString());

    for (const auto& id: objectTypeIds)
        query.addQueryItem("objectTypeId", id);

    // Start Time
    if (startTimeMs)
        query.addQueryItem("startTimeMs", QString::number(*startTimeMs));

    // End Time
    if (endTimeMs)
        query.addQueryItem("endTimeMs", QString::number(*endTimeMs));

    // Bounding Box
    if (boundingBox && !boundingBox->empty())
        query.addQueryItem("boundingBox", QString::fromStdString(*boundingBox));

    // Free Text
    if (!freeText.empty())
        query.addQueryItem("freeText", QString::fromStdString(freeText));

    if (!textScope.empty())
        query.addQueryItem("textScope", textScope);

    if (!referenceBestShotId.isNull())
        query.addQueryItem("referenceBestShotId", referenceBestShotId.toSimpleString());

    // Engine ID
    if (!engineId.isNull())
        query.addQueryItem("engineId", engineId.toSimpleString());

    // Site ID
    if (!siteId.empty())
        query.addQueryItem("siteId", QString::fromStdString(siteId));

    // TODO: cloud service should auto fill it according to the authorization.
    query.addQueryItem("organizationId", "");

    // Limit
    if (limit > 0)
        query.addQueryItem("limit", QString::number(limit));

    // Offset
    if (offset > 0)
        query.addQueryItem("offset", QString::number(offset));

    return query;
}

} // namespace nx::cloud::db::api
