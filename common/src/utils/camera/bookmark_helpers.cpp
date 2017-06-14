
#include "bookmark_helpers.h"

#include <utils/math/math.h>
#include <core/resource/camera_bookmark.h>
#include <core/resource/security_cam_resource.h>
#include <business/business_strings_helper.h>
#include <business/actions/abstract_business_action.h>

namespace
{
    bool isChangedEnough(qint64 first
        , qint64 second
        , qint64 minDiff)
    {
        return (std::abs(first - second) >= minDiff);
    }
}

QnCameraBookmark helpers::bookmarkFromAction(
    const QnAbstractBusinessActionPtr& action,
    const QnSecurityCamResourcePtr& camera,
    QnCommonModule* commonModule)
{
    if (!camera)
    {
        NX_EXPECT(false, "Camera is invalid");
        return QnCameraBookmark();
    }

    const auto actionParams = action->getParams();
    if (action->actionType() != QnBusiness::BookmarkAction
        && actionParams.targetActionType != QnBusiness::BookmarkAction)
    {
        NX_EXPECT(false, "Invalid action");
        return QnCameraBookmark();
    }

    const qint64 recordBeforeMs = actionParams.recordBeforeMs;
    const qint64 recordAfterMs = actionParams.recordAfter;
    const qint64 fixedDurationMs = actionParams.durationMs;
    const qint64 startTimeMs = action->getRuntimeParams().eventTimestampUsec / 1000;
    const qint64 endTimeMs = startTimeMs;

    QnCameraBookmark bookmark;
    bookmark.guid = QnUuid::createUuid();
    bookmark.startTimeMs = startTimeMs - recordBeforeMs;
    bookmark.durationMs = fixedDurationMs > 0 ? fixedDurationMs : endTimeMs - startTimeMs;
    bookmark.durationMs += recordBeforeMs + recordAfterMs;
    bookmark.cameraId = camera->getId();

    QnBusinessStringsHelper helper(commonModule);
    bookmark.name = helper.eventAtResource(action->getRuntimeParams(), Qn::RI_WithUrl);
    bookmark.description = helper.eventDetails(action->getRuntimeParams()).join(L'\n');
    bookmark.tags = action->getParams().tags.split(L',', QString::SkipEmptyParts).toSet();
    return bookmark;
}

QnCameraBookmarkList helpers::bookmarksAtPosition(const QnCameraBookmarkList &bookmarks
                                         , qint64 posMs)
{
    const auto predicate = [posMs](const QnCameraBookmark &bookmark)
    {
        return qBetween(bookmark.startTimeMs, posMs, bookmark.endTimeMs());
    };

    QnCameraBookmarkList result;
    std::copy_if(bookmarks.cbegin(), bookmarks.cend(), std::back_inserter(result), predicate);
    return result;
};

bool helpers::isTimeWindowChanged(qint64 firstStartTimeMs
    , qint64 firstEndTimeMs
    , qint64 secondStartTimeMs
    , qint64 secondEndTimeMs
    , qint64 minStep)
{
    return (isChangedEnough(firstStartTimeMs, secondStartTimeMs, minStep)
        || isChangedEnough(firstEndTimeMs, secondEndTimeMs, minStep));
}

QnTimePeriod helpers::extendTimeWindow(qint64 startTimeMs
    , qint64 endTimeMs
    , qint64 startTimeShiftMs
    , qint64 endTimeShiftMs)
{
    startTimeMs = (startTimeMs > startTimeShiftMs
        ? startTimeMs - startTimeShiftMs : QnTimePeriod::kMinTimeValue);

    endTimeMs = (QnTimePeriod::kMaxTimeValue - endTimeShiftMs > endTimeMs
        ? endTimeMs + endTimeShiftMs : QnTimePeriod::kMaxTimeValue);

    return QnTimePeriod::fromInterval(startTimeMs, endTimeMs);
}
