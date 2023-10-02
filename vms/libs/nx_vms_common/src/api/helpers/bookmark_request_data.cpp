// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "bookmark_request_data.h"

#include <chrono>

#include <common/common_globals.h>

#include <core/resource/camera_resource.h>
#include <core/resource_management/resource_pool.h>

#include <nx/fusion/model_functions.h>
#include <nx/reflect/string_conversion.h>
#include <utils/common/util.h>
#include <nx/utils/string.h>
#include <nx/utils/qt_helpers.h>
#include <nx/network/rest/params.h>
#include <nx/vms/event/actions/abstract_action.h>

#include <api/helpers/camera_id_helper.h>

using std::chrono::milliseconds;

namespace {

static const QString kStartTimeParam = lit("startTime");
static const QString kStartTimeMsParam = lit("startTimeMs");
static const QString kEndTimeParam = lit("endTime");
static const QString kEndTimeMsParam = lit("endTimeMs");
static const QString kSortColumnParam = lit("sortBy");
static const QString kSortOrderParam = lit("sortOrder");
static const QString kMinVisibleLengthParam = lit("minVisibleLength");
static const QString kFilterParam = lit("filter");
static const QString kDeprecatedPhysicalIdParam = lit("physicalId");
static const QString kDeprecatedMacParam = lit("mac");
static const QString kCameraIdParam = lit("cameraId");
static const QString kLimitParam = lit("limit");
static const QString kNameParam = lit("name");
static const QString kDescriptionParam = lit("description");
static const QString kTimeoutParam = lit("timeout");
static const QString kDurationParam = lit("duration");
static const QString kDurationMsParam = lit("durationMs");
static const QString kTagParam = lit("tag");
static const QString kEventRuleIdParam = lit("rule_id");
static const QString kCentralTimePointMs = lit("centralTimePointMs");

static const qint64 kUsPerMs = 1000;

nx::network::rest::Params bookmarksToParam(const QnCameraBookmark& bookmark)
{
    nx::network::rest::Params result;

    result.insert(QnCameraBookmark::kGuidParam, QnLexical::serialized(bookmark.guid));
    result.insert(kNameParam, bookmark.name);
    result.insert(kDescriptionParam, bookmark.description);
    result.insert(kTimeoutParam, QnLexical::serialized(bookmark.timeout));
    result.insert(kStartTimeMsParam, QnLexical::serialized(bookmark.startTimeMs));
    result.insert(kDurationMsParam, QnLexical::serialized(bookmark.durationMs));
    result.insert(kCameraIdParam, bookmark.cameraId.toString());

    for (const QString& tag: bookmark.tags)
        result.insert(kTagParam, tag);

    return result;
}

QnCameraBookmark bookmarkFromParams(
    const nx::network::rest::Params& params, QnResourcePool* resourcePool)
{
    QnCameraBookmark bookmark;
    bookmark.guid = QnLexical::deserialized<QnUuid>(params.value(QnCameraBookmark::kGuidParam));
    bookmark.name = params.value(kNameParam);
    bookmark.description = params.value(kDescriptionParam);
    bookmark.timeout = QnLexical::deserialized<milliseconds>(params.value(kTimeoutParam));
    bookmark.startTimeMs = QnLexical::deserialized<milliseconds>(params.value(kStartTimeMsParam, params.value(kStartTimeParam)));
    bookmark.durationMs = QnLexical::deserialized<milliseconds>(params.value(kDurationMsParam, params.value(kDurationParam)));

    QnSecurityCamResourcePtr camera = nx::camera_id_helper::findCameraByFlexibleIds(
        resourcePool,
        /*outNotFoundCameraId*/ nullptr,
        params,
        {kCameraIdParam, kDeprecatedPhysicalIdParam, kDeprecatedMacParam});
    if (!camera)
        bookmark.cameraId = QnUuid();
    else
        bookmark.cameraId = camera->getId();

    bookmark.tags = nx::utils::toQSet(params.values(kTagParam));
    return bookmark;
}

} // namespace

//-------------------------------------------------------------------------------------------------
// QnGetBookmarksRequestData

void QnGetBookmarksRequestData::loadFromParams(
    QnResourcePool* resourcePool, const nx::network::rest::Params& params)
{
    QnMultiserverRequestData::loadFromParams(resourcePool, params);

    if (params.contains(kStartTimeMsParam) || params.contains(kStartTimeParam))
    {
        filter.startTimeMs = milliseconds(nx::utils::parseDateTimeMsec(
            params.value(kStartTimeMsParam, params.value(kStartTimeParam))));
    }

    if (params.contains(kEndTimeMsParam) || params.contains(kEndTimeParam))
    {
        filter.endTimeMs = milliseconds(nx::utils::parseDateTimeMsec(
            params.value(kEndTimeMsParam, params.value(kEndTimeParam))));
    }

    nx::reflect::fromString(params.value(kSortColumnParam).toStdString(), &filter.orderBy.column);
    nx::reflect::fromString(params.value(kSortOrderParam).toStdString(), &filter.orderBy.order);

    if (milliseconds value; QnLexical::deserialize(params.value(kMinVisibleLengthParam), &value))
        filter.minVisibleLengthMs = value;

    if (params.contains(kLimitParam))
        filter.limit = qMax(0LL, params.value(kLimitParam).toLongLong());

    if (params.contains(kCentralTimePointMs))
    {
        filter.centralTimePointMs = milliseconds(
            nx::utils::parseDateTimeMsec(params.value(kCentralTimePointMs)));
    }

    filter.text = params.value(kFilterParam);
    if (QString param = params.value(QnCameraBookmark::kGuidParam); !param.isEmpty())
    {
        bool success = true;
        auto id = QnLexical::deserialized<QnUuid>(std::move(param), QnUuid(), &success);
        if (success)
            filter.id = std::move(id);
    }

    QnVirtualCameraResourceList cameras;
    nx::camera_id_helper::findAllCamerasByFlexibleIds(resourcePool, &cameras, params,
        {kCameraIdParam, kDeprecatedPhysicalIdParam, kDeprecatedMacParam});
    for (const auto& camera: cameras)
        filter.cameras.insert(camera->getId());

    if (QString param = params.value(QnCameraBookmark::kCreationStartTimeParam); !param.isEmpty())
        filter.creationStartTimeMs = milliseconds(nx::utils::parseDateTimeMsec(param));
    if (QString param = params.value(QnCameraBookmark::kCreationEndTimeParam); !param.isEmpty())
        filter.creationEndTimeMs = milliseconds(nx::utils::parseDateTimeMsec(param));
}

nx::network::rest::Params QnGetBookmarksRequestData::toParams() const
{
    auto result = QnMultiserverRequestData::toParams();

    result.insert(kStartTimeMsParam, QnLexical::serialized(filter.startTimeMs));
    result.insert(kEndTimeMsParam, QnLexical::serialized(filter.endTimeMs));
    result.insert(kFilterParam, QnLexical::serialized(filter.text));
    result.insert(kLimitParam, QnLexical::serialized(filter.limit));

    result.insert(kSortColumnParam, nx::reflect::toString(filter.orderBy.column));
    result.insert(kSortOrderParam, nx::reflect::toString(filter.orderBy.order));

    if (filter.minVisibleLengthMs)
        result.insert(kMinVisibleLengthParam, QnLexical::serialized(*filter.minVisibleLengthMs));

    for (const auto& cameraId: filter.cameras)
        result.insert(kCameraIdParam, QnLexical::serialized(cameraId));

    if (filter.id && !filter.id->isNull())
        result.insert(QnCameraBookmark::kGuidParam, QnLexical::serialized(*filter.id));

    if (filter.centralTimePointMs)
        result.insert(kCentralTimePointMs, QnLexical::serialized(*filter.centralTimePointMs));

    return result;
}

bool QnGetBookmarksRequestData::isValid() const
{
    return format != Qn::UnsupportedFormat;
}

//-------------------------------------------------------------------------------------------------
// QnGetBookmarkTagsRequestData

QnGetBookmarkTagsRequestData::QnGetBookmarkTagsRequestData(int limit):
    QnMultiserverRequestData(),
    limit(limit)
{
}

int QnGetBookmarkTagsRequestData::unlimited()
{
    return std::numeric_limits<int>().max();
}

void QnGetBookmarkTagsRequestData::loadFromParams(
    QnResourcePool* resourcePool, const nx::network::rest::Params& params)
{
    QnMultiserverRequestData::loadFromParams(resourcePool, params);
    limit = QnLexical::deserialized<int>(params.value(kLimitParam), unlimited());
}

nx::network::rest::Params QnGetBookmarkTagsRequestData::toParams() const
{
    auto result = QnMultiserverRequestData::toParams();
    result.insert(kLimitParam, QnLexical::serialized(limit));
    return result;
}

bool QnGetBookmarkTagsRequestData::isValid() const
{
    return limit >= 0;
}

//-------------------------------------------------------------------------------------------------
// QnUpdateBookmarkRequestData

QnUpdateBookmarkRequestData::QnUpdateBookmarkRequestData():
    QnMultiserverRequestData(),
    bookmark()
{
}

QnUpdateBookmarkRequestData::QnUpdateBookmarkRequestData(
    const QnCameraBookmark& bookmark,
    const QnUuid& eventRuleId)
    :
    QnMultiserverRequestData(),
    bookmark(bookmark),
    eventRuleId(eventRuleId)
{
}

void QnUpdateBookmarkRequestData::loadFromParams(
    QnResourcePool* resourcePool, const nx::network::rest::Params& params)
{
    QnMultiserverRequestData::loadFromParams(resourcePool, params);
    bookmark = bookmarkFromParams(params, resourcePool);
    if (params.contains(kEventRuleIdParam))
    {
        const auto stringRuleId = params.value(kEventRuleIdParam);
        eventRuleId = QnLexical::deserialized<QnUuid>(stringRuleId);
    }
}

nx::network::rest::Params QnUpdateBookmarkRequestData::toParams() const
{
    auto result = QnMultiserverRequestData::toParams();

    result.unite(bookmarksToParam(bookmark));
    result.insert(kEventRuleIdParam, QnLexical::serialized(eventRuleId));
    return result;
}

bool QnUpdateBookmarkRequestData::isValid() const
{
    return bookmark.isValid();
}

//-------------------------------------------------------------------------------------------------
// QnDeleteBookmarkRequestData

QnDeleteBookmarkRequestData::QnDeleteBookmarkRequestData():
    QnMultiserverRequestData(),
    bookmarkId()
{
}

QnDeleteBookmarkRequestData::QnDeleteBookmarkRequestData(const QnUuid& bookmarkId):
    QnMultiserverRequestData(),
    bookmarkId(bookmarkId)
{
}

void QnDeleteBookmarkRequestData::loadFromParams(
    QnResourcePool* resourcePool, const nx::network::rest::Params& params)
{
    QnMultiserverRequestData::loadFromParams(resourcePool, params);
    bookmarkId = QnLexical::deserialized<QnUuid>(params.value(QnCameraBookmark::kGuidParam));
}

nx::network::rest::Params QnDeleteBookmarkRequestData::toParams() const
{
    auto result = QnMultiserverRequestData::toParams();
    result.insert(QnCameraBookmark::kGuidParam, QnLexical::serialized(bookmarkId));
    return result;
}

bool QnDeleteBookmarkRequestData::isValid() const
{
    return !bookmarkId.isNull();
}
