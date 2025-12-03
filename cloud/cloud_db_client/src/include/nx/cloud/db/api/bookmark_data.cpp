// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "bookmark_data.h"

namespace nx::cloud::db::api {

nx::UrlQuery GetBookmarkFilter::toUrlQuery() const
{
    nx::UrlQuery query;

    if (organizationId)
        query.addQueryItem("organizationId", QString::fromStdString(*organizationId));

    if (deviceId)
    {
        for (const auto& id : *deviceId)
            query.addQueryItem("deviceId", QString::fromStdString(id));
    }

    if (siteId)
    {
        for (const auto& id : *siteId)
            query.addQueryItem("siteId", QString::fromStdString(id));
    }

    if (id)
        query.addQueryItem("id", QString::fromStdString(*id));

    if (startTimeMs)
        query.addQueryItem("startTimeMs", QString::number(startTimeMs->count()));

    if (endTimeMs)
        query.addQueryItem("endTimeMs", QString::number(endTimeMs->count()));

    if (text)
        query.addQueryItem("text", QString::fromStdString(*text));

    if (firstItemNum)
        query.addQueryItem("firstItemNum", QString::number(*firstItemNum));

    if (limit)
        query.addQueryItem("limit", QString::number(*limit));

    if (order)
        query.addQueryItem("order", QString::fromStdString(*order));

    if (column)
        query.addQueryItem("column", QString::fromStdString(*column));

    if (minVisibleLengthMs)
        query.addQueryItem("minVisibleLengthMs", QString::number(minVisibleLengthMs->count()));

    if (creationStartTimeMs)
        query.addQueryItem("creationStartTimeMs", QString::number(creationStartTimeMs->count()));

    if (creationEndTimeMs)
        query.addQueryItem("creationEndTimeMs", QString::number(creationEndTimeMs->count()));

    return query;
}

nx::UrlQuery GetBookmarkTagFilter::toUrlQuery() const
{
    nx::UrlQuery query;

    if (organizationId)
        query.addQueryItem("organizationId", QString::fromStdString(*organizationId));

    if (siteId)
    {
        for (const auto& id : *siteId)
            query.addQueryItem("siteId", QString::fromStdString(id));
    }

    if (limit)
        query.addQueryItem("limit", QString::number(*limit));

    return query;
}

nx::UrlQuery DeleteBookmarkFilter::toUrlQuery() const
{
    nx::UrlQuery query;

    query.addQueryItem("organizationId", QString::fromStdString(organizationId));

    for (const auto& id : siteId)
        query.addQueryItem("siteId", QString::fromStdString(id));

    if (deviceId)
    {
        for (const auto& id : *deviceId)
            query.addQueryItem("deviceId", QString::fromStdString(id));
    }

    if (id)
        query.addQueryItem("id", QString::fromStdString(*id));

    if (startTimeMs)
        query.addQueryItem("startTimeMs", QString::number(startTimeMs->count()));

    if (endTimeMs)
        query.addQueryItem("endTimeMs", QString::number(endTimeMs->count()));

    if (text)
        query.addQueryItem("text", QString::fromStdString(*text));

    return query;
}

} // namespace nx::cloud::db::api
