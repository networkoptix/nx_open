#include "bookmark_search_synchronizer.h"

#include <QtWidgets/QAction>

#include <core/resource/camera_resource.h>
#include <ui/graphics/items/resource/media_resource_widget.h>
#include <ui/workbench/watchers/timeline_bookmarks_watcher.h>
#include <ui/workbench/workbench_context.h>
#include <ui/workbench/workbench_navigator.h>

#include <nx/utils/log/assert.h>
#include <nx/vms/client/desktop/event_search/widgets/bookmark_search_widget.h>
#include <nx/vms/client/desktop/ui/actions/actions.h>
#include <nx/vms/client/desktop/ui/actions/action_manager.h>

namespace nx::vms::client::desktop {

BookmarkSearchSynchronizer::BookmarkSearchSynchronizer(
    QnWorkbenchContext* context,
    BookmarkSearchWidget* bookmarkSearchWidget,
    QObject* parent)
    :
    AbstractSearchSynchronizer(context, parent),
    m_bookmarkSearchWidget(bookmarkSearchWidget)
{
    NX_CRITICAL(m_bookmarkSearchWidget);

    connect(bookmarksAction(), &QAction::toggled, this, &BookmarkSearchSynchronizer::setActive);

    connect(this, &AbstractSearchSynchronizer::activeChanged,
        bookmarksAction(), &QAction::setChecked);

    connect(m_bookmarkSearchWidget.data(), &BookmarkSearchWidget::cameraSetChanged,
        this, &BookmarkSearchSynchronizer::updateTimelineBookmarks);

    connect(navigator(), &QnWorkbenchNavigator::currentResourceChanged,
        this, &BookmarkSearchSynchronizer::updateTimelineBookmarks);
}

QAction* BookmarkSearchSynchronizer::bookmarksAction() const
{
    return action(ui::action::BookmarksModeAction);
}

void BookmarkSearchSynchronizer::updateTimelineBookmarks()
{
    auto watcher = context()->instance<QnTimelineBookmarksWatcher>();
    if (!watcher || !m_bookmarkSearchWidget)
        return;

    const auto camera = navigator()->currentResource().dynamicCast<QnVirtualCameraResource>();
    const bool relevant = camera && m_bookmarkSearchWidget->cameras().contains(camera);

    watcher->setTextFilter(relevant ? m_bookmarkSearchWidget->textFilter() : QString());
};

} // namespace nx::vms::client::desktop
