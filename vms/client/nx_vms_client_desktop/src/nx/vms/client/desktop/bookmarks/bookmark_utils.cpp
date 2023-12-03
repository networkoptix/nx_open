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
#include <nx/vms/common/bookmark/bookmark_facade.h>

namespace nx::vms::client::desktop {

QString getVisibleBookmarkCreatorName(
    const QnCameraBookmark& bookmark,
    SystemContext* systemContext)
{
    const auto currentUser = systemContext->userWatcher()->user();

    if (!NX_ASSERT(currentUser))
        return {};

    if (bookmark.creatorId == QnCameraBookmark::systemUserId()
        || systemContext->resourceAccessManager()->hasPowerUserPermissions(currentUser)
        || currentUser->getId() == bookmark. creatorId)
    {
        return common::QnBookmarkFacade::creatorName(bookmark, systemContext->resourcePool());
    }

    return {}; //< Only power users can see name of other users.
}

} // namespace nx::vms::client::desktop
