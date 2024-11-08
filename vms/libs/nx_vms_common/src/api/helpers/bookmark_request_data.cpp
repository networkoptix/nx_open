// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "bookmark_request_data.h"

#include <chrono>

#include <api/helpers/camera_id_helper.h>
#include <common/common_globals.h>
#include <core/resource/camera_resource.h>
#include <core/resource_management/resource_pool.h>
#include <nx/fusion/model_functions.h>
#include <nx/network/rest/params.h>
#include <nx/reflect/string_conversion.h>
#include <nx/utils/qt_helpers.h>
#include <nx/utils/std_helpers.h>
#include <nx/utils/string.h>
#include <nx/vms/api/data/bookmark_models.h>
#include <nx/vms/event/actions/abstract_action.h>
#include <utils/common/util.h>

using std::chrono::milliseconds;

namespace {

const QString kStartTimeParam = lit("startTime");
const QString kStartTimeMsParam = lit("startTimeMs");
const QString kEndTimeParam = lit("endTime");
const QString kEndTimeMsParam = lit("endTimeMs");
const QString kSortColumnParam = lit("sortBy");
const QString kSortOrderParam = lit("sortOrder");
const QString kMinVisibleLengthParam = lit("minVisibleLength");
const QString kFilterParam = lit("filter");
const QString kDeprecatedPhysicalIdParam = lit("physicalId");
const QString kDeprecatedMacParam = lit("mac");
const QString kCameraIdParam = lit("cameraId");
const QString kLimitParam = lit("limit");
const QString kNameParam = lit("name");
const QString kDescriptionParam = lit("description");
const QString kTimeoutParam = lit("timeout");
const QString kDurationParam = lit("duration");
const QString kDurationMsParam = lit("durationMs");
const QString kTagParam = lit("tag");
const QString kEventRuleIdParam = lit("rule_id");
const QString kCentralTimePointMs = lit("centralTimePointMs");

nx::network::rest::Params bookmarksToParam(const nx::vms::common::CameraBookmark& bookmark)
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
    nx::vms::common::CameraBookmark bookmark;
    bookmark.guid = QnLexical::deserialized<nx::Uuid>(params.value(QnCameraBookmark::kGuidParam));
    bookmark.name = params.value(kNameParam);
    bookmark.description = params.value(kDescriptionParam);
    bookmark.timeout = QnLexical::deserialized<milliseconds>(params.value(kTimeoutParam));
    bookmark.startTimeMs = QnLexical::deserialized<milliseconds>(
        params.value(kStartTimeMsParam, params.value(kStartTimeParam)));
    bookmark.durationMs = QnLexical::deserialized<milliseconds>(
        params.value(kDurationMsParam, params.value(kDurationParam)));

    QnSecurityCamResourcePtr camera = nx::camera_id_helper::findCameraByFlexibleIds(
        resourcePool,
        /*outNotFoundCameraId*/ nullptr,
        params,
        {kCameraIdParam, kDeprecatedPhysicalIdParam, kDeprecatedMacParam});
    if (!camera)
        bookmark.cameraId = nx::Uuid();
    else
        bookmark.cameraId = camera->getId();

    bookmark.tags = nx::utils::toQSet(params.values(kTagParam));
    return bookmark;
}

} // namespace

namespace nx::vms::common {
//-------------------------------------------------------------------------------------------------
// GetBookmarksRequestData

void nx::vms::common::GetBookmarksRequestData::loadFromParams(
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
        filter.centralTimePointMs =
            milliseconds(nx::utils::parseDateTimeMsec(params.value(kCentralTimePointMs)));
    }

    filter.text = params.value(kFilterParam);
    if (QString param = params.value(QnCameraBookmark::kGuidParam); !param.isEmpty())
    {
        bool success = true;
        auto id = QnLexical::deserialized<nx::Uuid>(param, nx::Uuid(), &success);
        if (success)
            filter.id = id;
    }

    QnVirtualCameraResourceList cameras;
    nx::camera_id_helper::findAllCamerasByFlexibleIds(resourcePool,
        &cameras,
        params,
        {kCameraIdParam, kDeprecatedPhysicalIdParam, kDeprecatedMacParam});
    for (const auto& camera: cameras)
        filter.cameras.insert(camera->getId());

    if (QString param = params.value(QnCameraBookmark::kCreationStartTimeParam); !param.isEmpty())
        filter.creationStartTimeMs = milliseconds(nx::utils::parseDateTimeMsec(param));
    if (QString param = params.value(QnCameraBookmark::kCreationEndTimeParam); !param.isEmpty())
        filter.creationEndTimeMs = milliseconds(nx::utils::parseDateTimeMsec(param));
}

nx::network::rest::Params nx::vms::common::GetBookmarksRequestData::toParams() const
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

bool nx::vms::common::GetBookmarksRequestData::isValid() const
{
    return format != Qn::SerializationFormat::unsupported;
}

//-------------------------------------------------------------------------------------------------
// GetBookmarkTagsRequestData

nx::vms::common::GetBookmarkTagsRequestData::GetBookmarkTagsRequestData(int limit):
    limit(limit)
{
}

int nx::vms::common::GetBookmarkTagsRequestData::unlimited()
{
    return std::numeric_limits<int>::max();
}

void GetBookmarkTagsRequestData::loadFromParams(
    QnResourcePool* resourcePool, const nx::network::rest::Params& params)
{
    QnMultiserverRequestData::loadFromParams(resourcePool, params);
    limit = QnLexical::deserialized<int>(params.value(kLimitParam), unlimited());
}

nx::network::rest::Params GetBookmarkTagsRequestData::toParams() const
{
    auto result = QnMultiserverRequestData::toParams();
    result.insert(kLimitParam, QnLexical::serialized(limit));
    return result;
}

bool GetBookmarkTagsRequestData::isValid() const
{
    return limit >= 0;
}

//-------------------------------------------------------------------------------------------------
// UpdateBookmarkRequestData

UpdateBookmarkRequestData::UpdateBookmarkRequestData(): bookmark()
{
}

UpdateBookmarkRequestData::UpdateBookmarkRequestData(
    CameraBookmark bookmark, const nx::Uuid& eventRuleId)
    :
    bookmark(std::move(bookmark)),
    eventRuleId(eventRuleId)
{
}

void UpdateBookmarkRequestData::loadFromParams(
    QnResourcePool* resourcePool, const nx::network::rest::Params& params)
{
    QnMultiserverRequestData::loadFromParams(resourcePool, params);
    bookmark = bookmarkFromParams(params, resourcePool);
    if (params.contains(kEventRuleIdParam))
    {
        const auto stringRuleId = params.value(kEventRuleIdParam);
        eventRuleId = QnLexical::deserialized<nx::Uuid>(stringRuleId);
    }
}

nx::network::rest::Params UpdateBookmarkRequestData::toParams() const
{
    auto result = QnMultiserverRequestData::toParams();

    result.unite(bookmarksToParam(bookmark));
    result.insert(kEventRuleIdParam, QnLexical::serialized(eventRuleId));
    return result;
}

bool UpdateBookmarkRequestData::isValid() const
{
    return bookmark.isValid();
}

//-------------------------------------------------------------------------------------------------
// DeleteBookmarkRequestData

DeleteBookmarkRequestData::DeleteBookmarkRequestData(): bookmarkId()
{
}

DeleteBookmarkRequestData::DeleteBookmarkRequestData(const nx::Uuid& bookmarkId):
    bookmarkId(bookmarkId)
{
}

void DeleteBookmarkRequestData::loadFromParams(
    QnResourcePool* resourcePool, const nx::network::rest::Params& params)
{
    QnMultiserverRequestData::loadFromParams(resourcePool, params);
    bookmarkId = QnLexical::deserialized<nx::Uuid>(params.value(QnCameraBookmark::kGuidParam));
}

nx::network::rest::Params DeleteBookmarkRequestData::toParams() const
{
    auto result = QnMultiserverRequestData::toParams();
    result.insert(QnCameraBookmark::kGuidParam, QnLexical::serialized(bookmarkId));
    return result;
}

bool DeleteBookmarkRequestData::isValid() const
{
    return !bookmarkId.isNull();
}

} // namespace nx::vms::common
