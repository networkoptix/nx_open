// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "bookmark_search_synchronizer.h"

#include <QtGui/QAction>

#include <core/resource/camera_resource.h>
#include <nx/utils/log/assert.h>
#include <nx/vms/client/core/event_search/models/abstract_search_list_model.h>
#include <nx/vms/client/core/event_search/utils/text_filter_setup.h>
#include <nx/vms/client/desktop/event_search/utils/common_object_search_setup.h>
#include <nx/vms/client/desktop/menu/action_manager.h>
#include <nx/vms/client/desktop/menu/actions.h>
#include <ui/graphics/items/resource/media_resource_widget.h>
#include <ui/workbench/watchers/timeline_bookmarks_watcher.h>
#include <ui/workbench/watchers/workbench_item_bookmarks_watcher.h>
#include <ui/workbench/workbench_context.h>
#include <ui/workbench/workbench_navigator.h>

namespace nx::vms::client::desktop {

BookmarkSearchSynchronizer::BookmarkSearchSynchronizer(
    WindowContext* context,
    CommonObjectSearchSetup* searchSetup,
    QObject* parent)
    :
    AbstractSearchSynchronizer(context, parent),
    m_searchSetup(searchSetup)
{
    NX_CRITICAL(m_searchSetup);

    connect(bookmarksAction(), &QAction::toggled, this, &BookmarkSearchSynchronizer::setActive);

    connect(this, &AbstractSearchSynchronizer::activeChanged,
        bookmarksAction(), &QAction::setChecked);

    connect(m_searchSetup.data(), &CommonObjectSearchSetup::selectedCamerasChanged,
        this, &BookmarkSearchSynchronizer::updateTimelineBookmarks);

    connect(navigator(), &QnWorkbenchNavigator::currentResourceChanged,
        this, &BookmarkSearchSynchronizer::updateTimelineBookmarks);

    connect(m_searchSetup->textFilter(), &core::TextFilterSetup::textChanged,
        this, &BookmarkSearchSynchronizer::updateTimelineBookmarks);
}

QAction* BookmarkSearchSynchronizer::bookmarksAction() const
{
    return action(menu::BookmarksModeAction);
}

void BookmarkSearchSynchronizer::updateTimelineBookmarks()
{
    if (!m_searchSetup)
        return;

    const auto camera = navigator()->currentResource().dynamicCast<QnVirtualCameraResource>();

    const bool relevant = camera && m_searchSetup->selectedCameras().contains(camera);

    const auto cameraSetType = m_searchSetup->cameraSelection();
    const auto cameras = cameraSetType == core::EventSearch::CameraSelection::current
        ? m_searchSetup->selectedCameras()
        : QnVirtualCameraResourceSet();

    const auto filterText = NX_ASSERT(m_searchSetup->textFilter())
        ? m_searchSetup->textFilter()->text()
        : QString();

    if (auto watcher = workbenchContext()->instance<QnTimelineBookmarksWatcher>())
        watcher->setTextFilter(relevant ? filterText : QString());

    if (auto watcher = workbenchContext()->instance<QnWorkbenchItemBookmarksWatcher>())
        watcher->setDisplayFilter(cameras, filterText);
};

} // namespace nx::vms::client::desktop
