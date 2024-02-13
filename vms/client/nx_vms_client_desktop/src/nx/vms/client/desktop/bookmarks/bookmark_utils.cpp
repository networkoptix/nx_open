// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "bookmark_utils.h"

#include <QtCore/QCoreApplication>

#include <core/resource/camera_bookmark.h>
#include <core/resource/user_resource.h>
#include <core/resource_access/resource_access_manager.h>
#include <core/resource_access/resource_access_subject.h>
#include <core/resource_management/resource_pool.h>
#include <nx/vms/client/core/watchers/user_watcher.h>
#include <nx/vms/client/desktop/system_context.h>

namespace {

struct BookmarkUtilStrings
{
    Q_DECLARE_TR_FUNCTIONS(BookmarkUtilStrings)
public:
    static QString systemEvent()
    {
        return tr("Site Event", "Shows that the bookmark was created by a site event");
    }
};

} // namespace

namespace nx::vms::client::desktop {

QString getBookmarkCreatorName(const nx::Uuid& creatorId, SystemContext* systemContext)
{
    const auto currentUser = systemContext->userWatcher()->user();

    if (!NX_ASSERT(currentUser))
        return {};

    if (creatorId.isNull())
        return {};

    if (creatorId == QnCameraBookmark::systemUserId())
    {
        return BookmarkUtilStrings::systemEvent();
    }

    if (!systemContext->resourceAccessManager()->hasPowerUserPermissions(currentUser)
        && currentUser->getId() != creatorId)
    {
        // Only power users can see name of other users.
        return {};
    }

    const auto userResource =
        systemContext->resourcePool()->getResourceById<QnUserResource>(creatorId);

    return userResource ? userResource->getName() : QString();
}
} // namespace nx::vms::client::desktop
