#include "bookmark_request_data.h"

#include <chrono>

#include <common/common_globals.h>

#include <core/resource/camera_resource.h>
#include <core/resource_management/resource_pool.h>

#include <nx/fusion/model_functions.h>
#include <utils/common/util.h>
#include <nx/utils/string.h>
#include <nx/vms/event/actions/abstract_action.h>

#include <api/helpers/camera_id_helper.h>

using std::chrono::milliseconds;

namespace {

static const QString kStartTimeParam = lit("startTime");
static const QString kEndTimeParam = lit("endTime");
static const QString kSortColumnParam = lit("sortBy");
static const QString kSortOrderParam = lit("sortOrder");
static const QString kUseSparsingParam = lit("sparse");
static const QString kMinVisibleLengthParam = lit("minVisibleLength");
static const QString kFilterParam = lit("filter");
static const QString kDeprecatedPhysicalIdParam = lit("physicalId");
static const QString kDeprecatedMacParam = lit("mac");
static const QString kCameraIdParam = lit("cameraId");
static const QString kLimitParam = lit("limit");
static const QString kGuidParam = lit("guid");
static const QString kNameParam = lit("name");
static const QString kDescriptionParam = lit("description");
static const QString kTimeoutParam = lit("timeout");
static const QString kDurationParam = lit("duration");
static const QString kTagParam = lit("tag");
static const QString kEventRuleIdParam = lit("rule_id");

static const qint64 kUsPerMs = 1000;

QnRequestParamList bookmarksToParam(const QnCameraBookmark& bookmark)
{
    QnRequestParamList result;

    result.insert(kGuidParam, QnLexical::serialized(bookmark.guid));
    result.insert(kNameParam, bookmark.name);
    result.insert(kDescriptionParam, bookmark.description);
    result.insert(kTimeoutParam, QnLexical::serialized(bookmark.timeout));
    result.insert(kStartTimeParam, QnLexical::serialized(bookmark.startTimeMs));
    result.insert(kDurationParam, QnLexical::serialized(bookmark.durationMs));
    result.insert(kCameraIdParam, bookmark.cameraId.toString());

    for (const QString& tag: bookmark.tags)
        result.insert(kTagParam, tag);

    return result;
}

QnCameraBookmark bookmarkFromParams(const QnRequestParamList& params, QnResourcePool* resourcePool)
{
    QnCameraBookmark bookmark;
    bookmark.guid = QnLexical::deserialized<QnUuid>(params.value(kGuidParam));
    bookmark.name = params.value(kNameParam);
    bookmark.description = params.value(kDescriptionParam);
    bookmark.timeout = QnLexical::deserialized<milliseconds>(params.value(kTimeoutParam));
    bookmark.startTimeMs = QnLexical::deserialized<milliseconds>(params.value(kStartTimeParam));
    bookmark.durationMs = QnLexical::deserialized<milliseconds>(params.value(kDurationParam));

    QnSecurityCamResourcePtr camera = nx::camera_id_helper::findCameraByFlexibleIds(
        resourcePool,
        /*outNotFoundCameraId*/ nullptr,
        params.toHash(),
        {kCameraIdParam, kDeprecatedPhysicalIdParam, kDeprecatedMacParam});
    if (!camera)
        bookmark.cameraId = QnUuid();
    else
        bookmark.cameraId = camera->getId();

    bookmark.tags = params.allValues(kTagParam).toSet();
    return bookmark;
}

} // namespace

//-------------------------------------------------------------------------------------------------
// QnGetBookmarksRequestData

QnGetBookmarksRequestData::QnGetBookmarksRequestData():
    QnMultiserverRequestData(),
    filter(),
    cameras()
{
}

void QnGetBookmarksRequestData::loadFromParams(QnResourcePool* resourcePool, const QnRequestParamList& params)
{
    QnMultiserverRequestData::loadFromParams(resourcePool, params);

    if (params.contains(kStartTimeParam))
        filter.startTimeMs =
            milliseconds(nx::utils::parseDateTime(params.value(kStartTimeParam)) / kUsPerMs);

    if (params.contains(kEndTimeParam))
        filter.endTimeMs =
            milliseconds(nx::utils::parseDateTime(params.value(kEndTimeParam)))  / kUsPerMs;

    QnLexical::deserialize(params.value(kSortColumnParam), &filter.orderBy.column);
    QnLexical::deserialize(params.value(kSortOrderParam), &filter.orderBy.order);

    QnLexical::deserialize(params.value(kUseSparsingParam), &filter.sparsing.used);
    QnLexical::deserialize(params.value(kMinVisibleLengthParam),
        &filter.sparsing.minVisibleLengthMs);

    if (params.contains(kLimitParam))
        filter.limit = qMax(0LL, params.value(kLimitParam).toLongLong());

    filter.text = params.value(kFilterParam);

    nx::camera_id_helper::findAllCamerasByFlexibleIds(resourcePool, &cameras, params,
        {kCameraIdParam, kDeprecatedPhysicalIdParam, kDeprecatedMacParam});
}

QnRequestParamList QnGetBookmarksRequestData::toParams() const
{
    QnRequestParamList result = QnMultiserverRequestData::toParams();

    result.insert(kStartTimeParam, QnLexical::serialized(filter.startTimeMs));
    result.insert(kEndTimeParam, QnLexical::serialized(filter.endTimeMs));
    result.insert(kFilterParam, QnLexical::serialized(filter.text));
    result.insert(kLimitParam, QnLexical::serialized(filter.limit));

    result.insert(kSortColumnParam, QnLexical::serialized(filter.orderBy.column));
    result.insert(kSortOrderParam, QnLexical::serialized(filter.orderBy.order));

    result.insert(kUseSparsingParam, QnLexical::serialized(filter.sparsing.used));
    result.insert(kMinVisibleLengthParam,
        QnLexical::serialized(filter.sparsing.minVisibleLengthMs));

    for (const auto& camera: cameras)
        result.insert(kCameraIdParam, QnLexical::serialized(camera->getId()));

    return result;
}

bool QnGetBookmarksRequestData::isValid() const
{
    return !cameras.isEmpty()
        && filter.endTimeMs > filter.startTimeMs
        && format != Qn::UnsupportedFormat;
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

void QnGetBookmarkTagsRequestData::loadFromParams(QnResourcePool* resourcePool,
    const QnRequestParamList& params)
{
    QnMultiserverRequestData::loadFromParams(resourcePool, params);
    limit = QnLexical::deserialized<int>(params.value(kLimitParam), unlimited());
}

QnRequestParamList QnGetBookmarkTagsRequestData::toParams() const
{
    QnRequestParamList result = QnMultiserverRequestData::toParams();
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

void QnUpdateBookmarkRequestData::loadFromParams(QnResourcePool* resourcePool,
    const QnRequestParamList& params)
{
    QnMultiserverRequestData::loadFromParams(resourcePool, params);
    bookmark = bookmarkFromParams(params, resourcePool);
    if (params.contains(kEventRuleIdParam))
    {
        const auto stringRuleId = params.value(kEventRuleIdParam);
        eventRuleId = QnLexical::deserialized<QnUuid>(stringRuleId);
    }
}

QnRequestParamList QnUpdateBookmarkRequestData::toParams() const
{
    QnRequestParamList result = QnMultiserverRequestData::toParams();

    result.append(bookmarksToParam(bookmark));
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

void QnDeleteBookmarkRequestData::loadFromParams(QnResourcePool* resourcePool,
    const QnRequestParamList& params)
{
    QnMultiserverRequestData::loadFromParams(resourcePool, params);
    bookmarkId = QnLexical::deserialized<QnUuid>(params.value(kGuidParam));
}

QnRequestParamList QnDeleteBookmarkRequestData::toParams() const
{
    QnRequestParamList result = QnMultiserverRequestData::toParams();
    result.insert(kGuidParam, QnLexical::serialized(bookmarkId));
    return result;
}

bool QnDeleteBookmarkRequestData::isValid() const
{
    return !bookmarkId.isNull();
}
