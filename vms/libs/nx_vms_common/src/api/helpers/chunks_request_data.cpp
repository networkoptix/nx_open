// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "chunks_request_data.h"

#include <api/helpers/camera_id_helper.h>
#include <common/common_globals.h>
#include <common/common_module.h>
#include <core/resource/camera_resource.h>
#include <core/resource_management/resource_pool.h>
#include <nx/fusion/serialization/json_functions.h>
#include <nx/media/motion_detection.h>
#include <nx/network/rest/params.h>
#include <nx/reflect/string_conversion.h>
#include <nx/utils/string.h>
#include <nx_ec/abstract_ec_connection.h>

namespace {

const QString kStartTimeParam(lit("startTime"));
const QString kEndTimeParam(lit("endTime"));
const QString kDetailParam(lit("detail"));
const QString kKeepSmallChunksParam(lit("keepSmallChunks"));
const QString kPreciseBoundsParam(lit("preciseBounds"));
const QString kPeriodsTypeParam(lit("periodsType"));
const QString kFilterParam(lit("filter"));
const QString kLocalParam(lit("local"));
const QString kFormatParam(lit("format"));
const QString kDeprecatedPhysicalIdParam(lit("physicalId"));
const QString kDeprecatedMacParam(lit("mac"));
const QString kDeprecatedIdParam(lit("id"));
const QString kCameraIdParam(lit("cameraId"));
const QString kLimitParam(lit("limit"));
const QString kFlatParam(lit("flat"));
const QString kDescendingOrderParam(lit("desc"));
const QString kGroupByParamName(lit("groupBy"));
const QString kStorageLocationParamName("storageLocation");

bool isValidMotionRect(const QRect& rect)
{
    return rect.isEmpty()
        || (rect.left() >= 0 && rect.right() < Qn::kMotionGridWidth
            && rect.top() >= 0 && rect.bottom() < Qn::kMotionGridHeight);
};

bool isValidMotionRegion(const QRegion& region)
{
    return region.isNull()
        || std::all_of(region.cbegin(), region.cend(), isValidMotionRect);
};

bool isNullRegion(const QRegion& region)
{
    return region.isNull();
}

} // namespace

QnChunksRequestData QnChunksRequestData::fromParams(
    QnResourcePool* resourcePool, const nx::network::rest::Params& params)
{
    QnChunksRequestData request;

    if (params.contains(kStartTimeParam))
        request.startTimeMs = nx::utils::parseDateTimeMsec(params.value(kStartTimeParam));

    if (params.contains(kEndTimeParam))
        request.endTimeMs = nx::utils::parseDateTimeMsec(params.value(kEndTimeParam));

    if (params.contains(kDetailParam))
        request.detailLevel = std::chrono::milliseconds(params.value(kDetailParam).toLongLong());
    if (params.contains(kKeepSmallChunksParam))
        request.keepSmallChunks = true;
    if (params.contains(kPreciseBoundsParam))
        request.preciseBounds = true;

    if (params.contains(kPeriodsTypeParam))
    {
        request.periodsType = static_cast<Qn::TimePeriodContent>(
            params.value(kPeriodsTypeParam).toInt());
    }

    if (params.contains(kLimitParam))
        request.limit = qMax(0LL, params.value(kLimitParam).toLongLong());

    if (params.contains(kFlatParam))
    {
        request.groupBy = QnChunksRequestData::GroupBy::none;
    }
    else
    {
        request.groupBy = nx::reflect::fromString<QnChunksRequestData::GroupBy>(
            params.value(kGroupByParamName).toStdString(),
            QnChunksRequestData::GroupBy::serverId);
    }

    nx::reflect::fromString(params.value(kStorageLocationParamName).toStdString(), &request.storageLocation);

    request.sortOrder = params.contains(kDescendingOrderParam)
        ? Qt::SortOrder::DescendingOrder : Qt::SortOrder::AscendingOrder;

    request.filter = params.value(kFilterParam);
    request.isLocal = params.contains(kLocalParam);
    nx::reflect::fromString(params.value(kFormatParam).toStdString(), &request.format);

    nx::camera_id_helper::findAllCamerasByFlexibleIds(resourcePool, &request.resList, params,
        {kCameraIdParam, kDeprecatedIdParam, kDeprecatedPhysicalIdParam, kDeprecatedMacParam});

    return request;
}

nx::network::rest::Params QnChunksRequestData::toParams() const
{
    nx::network::rest::Params result;

    result.insert(kStartTimeParam, QString::number(startTimeMs));
    result.insert(kEndTimeParam, QString::number(endTimeMs));
    result.insert(kDetailParam, QString::number(detailLevel.count()));
    if (keepSmallChunks)
        result.insert(kKeepSmallChunksParam, QString());
    if (preciseBounds)
        result.insert(kPreciseBoundsParam, QString());
    result.insert(kPeriodsTypeParam, QString::number(periodsType));
    result.insert(kFilterParam, filter);
    result.insert(kLimitParam, QString::number(limit));
    if (sortOrder == Qt::SortOrder::DescendingOrder)
        result.insert(kDescendingOrderParam, QString());
    result.insert(kGroupByParamName, nx::reflect::toString(groupBy));
    result.insert(kStorageLocationParamName, nx::reflect::toString(storageLocation));

    if (isLocal)
        result.insert(kLocalParam, QString());
    result.insert(kFormatParam, format);
    for (const auto& resource: resList)
        result.insert(kCameraIdParam, resource->getId().toString());

    return result;
}

QUrlQuery QnChunksRequestData::toUrlQuery() const
{
    return toParams().toUrlQuery();
}

bool QnChunksRequestData::isValid() const
{
    if (resList.isEmpty()
        || endTimeMs <= startTimeMs
        || format == Qn::SerializationFormat::unsupported)
    {
        return false;
    }

    if (periodsType == Qn::MotionContent && !filter.trimmed().isEmpty())
    {
        const auto motionRegions = QJson::deserialized<QList<QRegion>>(filter.toUtf8());
        if (!std::any_of(motionRegions.cbegin(), motionRegions.cend(), isValidMotionRegion))
            return false;
        if (std::all_of(motionRegions.cbegin(), motionRegions.cend(), isNullRegion))
            return false;
    }

    return true;
}
