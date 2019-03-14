#include "chunks_request_data.h"

#include <common/common_globals.h>

#include <core/resource/camera_resource.h>
#include <core/resource_management/resource_pool.h>

#include <nx/fusion/model_functions.h>
#include <nx/fusion/serialization/lexical.h>
#include <nx/utils/string.h>

#include <api/helpers/camera_id_helper.h>

namespace {

const QString kStartTimeParam(lit("startTime"));
const QString kEndTimeParam(lit("endTime"));
const QString kDetailParam(lit("detail"));
const QString kKeepSmallChunksParam(lit("keepSmallChunks"));
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

bool isValidMotionRect(const QRect& rect)
{
    return !rect.isEmpty()
        && rect.left() >= 0
        && rect.top() >= 0
        && rect.right() < Qn::kMotionGridWidth
        && rect.bottom() < Qn::kMotionGridHeight;
};

bool isValidMotionRegion(const QRegion& region)
{
    return region.rectCount() > 0
        && std::all_of(region.cbegin(), region.cend(), isValidMotionRect);
};

} // namespace

QnChunksRequestData QnChunksRequestData::fromParams(QnResourcePool* resourcePool,
    const QnRequestParamList& params)
{
    static const qint64 kUsPerMs = 1000;

    QnChunksRequestData request;

    if (params.contains(kStartTimeParam))
        request.startTimeMs = nx::utils::parseDateTime(params.value(kStartTimeParam)) / kUsPerMs;

    if (params.contains(kEndTimeParam))
        request.endTimeMs = nx::utils::parseDateTime(params.value(kEndTimeParam)) / kUsPerMs;

    if (params.contains(kDetailParam))
        request.detailLevel = std::chrono::milliseconds(params.value(kDetailParam).toLongLong());
    if (params.contains(kKeepSmallChunksParam))
        request.keepSmallChunks = true;

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
        request.groupBy = QnLexical::deserialized<QnChunksRequestData::GroupBy>(
            params.value(kGroupByParamName),
            QnChunksRequestData::GroupBy::serverId);
    }
    request.sortOrder = params.contains(kDescendingOrderParam)
        ? Qt::SortOrder::DescendingOrder : Qt::SortOrder::AscendingOrder;

    request.filter = params.value(kFilterParam);
    request.isLocal = params.contains(kLocalParam);
    QnLexical::deserialize(params.value(kFormatParam), &request.format);

    nx::camera_id_helper::findAllCamerasByFlexibleIds(resourcePool, &request.resList, params,
        {kCameraIdParam, kDeprecatedIdParam, kDeprecatedPhysicalIdParam, kDeprecatedMacParam});

    if (request.periodsType == Qn::TimePeriodContent::AnalyticsContent)
    {
        request.analyticsStorageFilter = nx::analytics::storage::Filter();
        if (!deserializeFromParams(params, &request.analyticsStorageFilter.get()))
            request.analyticsStorageFilter.reset();
    }

    return request;
}

QnRequestParamList QnChunksRequestData::toParams() const
{
    QnRequestParamList result;

    result.insert(kStartTimeParam, QString::number(startTimeMs));
    result.insert(kEndTimeParam, QString::number(endTimeMs));
    result.insert(kDetailParam, QString::number(detailLevel.count()));
    if (keepSmallChunks)
        result.insert(kKeepSmallChunksParam, QString());
    result.insert(kPeriodsTypeParam, QString::number(periodsType));
    result.insert(kFilterParam, filter);
    result.insert(kLimitParam, QString::number(limit));
    if (sortOrder == Qt::SortOrder::DescendingOrder)
        result.insert(kDescendingOrderParam, QString());
    result.insert(kGroupByParamName, QnLexical::serialized(groupBy));

    if (isLocal)
        result.insert(kLocalParam, QString());
    result.insert(kFormatParam, QnLexical::serialized(format));

    // TODO: #mshevchenko #3.1 The request format changed in 3.0, so Mobile Client cannot
    // read chunks from 2.6 server. This is a temporary in-place fix which should be refactored
    // when API versioning is implemented.
    switch (requestVersion)
    {
        case RequestVersion::v2_6:
            for (const auto& resource: resList)
                result.insert(kDeprecatedPhysicalIdParam, resource->getPhysicalId());
            break;

        case RequestVersion::v3_0:
        default:
            for (const auto& resource: resList)
                result.insert(kCameraIdParam, resource->getId().toString());
            break;
    }

    return result;
}

QUrlQuery QnChunksRequestData::toUrlQuery() const
{
    QUrlQuery urlQuery;
    for (const auto& param: toParams())
        urlQuery.addQueryItem(param.first, param.second);
    return urlQuery;
}

bool QnChunksRequestData::isValid() const
{
    if (resList.isEmpty()
        || endTimeMs <= startTimeMs
        || format == Qn::UnsupportedFormat)
    {
        return false;
    }

    if (periodsType == Qn::MotionContent && !filter.trimmed().isEmpty())
    {
        const auto motionRegions = QJson::deserialized<QList<QRegion>>(filter.toUtf8());
        if (!std::all_of(motionRegions.cbegin(), motionRegions.cend(), isValidMotionRegion))
            return false;
    }

    return true;
}

QN_DEFINE_EXPLICIT_ENUM_LEXICAL_FUNCTIONS(QnChunksRequestData, GroupBy,
    (QnChunksRequestData::GroupBy::none, "none")
    (QnChunksRequestData::GroupBy::cameraId, "cameraId")
    (QnChunksRequestData::GroupBy::serverId, "serverId"))
