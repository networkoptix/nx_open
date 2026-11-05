// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "bookmark_data_helper.h"

#include <core/resource/resource.h>
#include <nx/vms/client/core/watchers/feature_access_watcher.h>
#include <nx/vms/client/mobile/system_context.h>

using namespace Qt::StringLiterals;

namespace nx::vms::client::mobile {
namespace timeline {

namespace {

bool isShared(const common::CameraBookmark& bookmark, const QnResourcePtr& resource)
{
    const auto systemContext = SystemContext::fromResource(resource);
    if (!systemContext || !systemContext->featureAccess())
        return false;

    constexpr api::BookmarkShareFilters kSharedBookmarkFilters =
        api::BookmarkShareFilter::shared | api::BookmarkShareFilter::accessible;

    return systemContext->featureAccess()->canUseShareBookmark() && bookmark.shareable()
        && bookmark.bookmarkMatchesFilter(kSharedBookmarkFilters);
}

} // namespace

ObjectData BookmarkDataHelper::makeObjectData(
    const common::CameraBookmark& bookmark, const QnResourcePtr& resource)
{
    return ObjectData{.id = bookmark.guid,
        .startTimeMs = bookmark.startTimeMs.count(),
        .durationMs = bookmark.durationMs.count(),
        .title = bookmark.name.isEmpty() ? tr("Bookmark") : bookmark.name,
        .description = bookmark.description,
        .imagePath =
            makeImageRequest(resource, bookmark.startTimeMs.count(), kHighImageResolution),
        .iconPath = iconPath(),
        .tags = QStringList{bookmark.tags.cbegin(), bookmark.tags.cend()},
        .shared = isShared(bookmark, resource),
        .source = QVariant::fromValue(bookmark)};
}

QString BookmarkDataHelper::iconPath()
{
    return u"image://skin/20x20/Solid/bookmark.svg"_s;
}

} // namespace timeline
} // namespace nx::vms::client::mobile
