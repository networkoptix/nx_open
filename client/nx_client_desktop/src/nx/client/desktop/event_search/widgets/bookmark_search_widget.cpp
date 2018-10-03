#include "bookmark_search_widget.h"

#include <core/resource/camera_resource.h>
#include <ui/style/skin.h>
#include <ui/workbench/watchers/timeline_bookmarks_watcher.h>
#include <ui/workbench/workbench_context.h>
#include <ui/workbench/workbench_navigator.h>

#include <nx/client/desktop/event_search/models/bookmark_search_list_model.h>
#include <nx/utils/log/assert.h>

namespace nx::client::desktop {

BookmarkSearchWidget::BookmarkSearchWidget(QnWorkbenchContext* context, QWidget* parent):
    base_type(context, new BookmarkSearchListModel(context), parent)
{
    setPlaceholderPixmap(qnSkin->pixmap("events/placeholders/bookmarks.png"));

    const auto updateTimelineBookmarks =
        [this]()
        {
            auto watcher = this->context()->instance<QnTimelineBookmarksWatcher>();
            if (!watcher)
                return;

            const auto cameras = this->cameras();
            const auto currentCamera = navigator()->currentResource()
                .dynamicCast<QnVirtualCameraResource>();
            const bool relevant = cameras.empty() || cameras.contains(currentCamera);
            watcher->setTextFilter(relevant ? textFilter() : QString());
        };

    connect(this, &AbstractSearchWidget::cameraSetChanged, updateTimelineBookmarks);

    connect(navigator(), &QnWorkbenchNavigator::currentResourceChanged,
        this, updateTimelineBookmarks);

    connect(this, &AbstractSearchWidget::textFilterChanged,
        [this, updateTimelineBookmarks](const QString& text)
        {
            auto bookmarksModel = qobject_cast<BookmarkSearchListModel*>(model());
            NX_ASSERT(bookmarksModel);
            if (!bookmarksModel)
                return;

            bookmarksModel->setFilterText(text);
            updateTimelineBookmarks();
            requestFetch();
        });
}

QString BookmarkSearchWidget::placeholderText(bool constrained) const
{
    static const QString kHtmlPlaceholder =
        lit("<center><p>%1</p><p><font size='-3'>%2</font></p></center>")
        .arg(tr("No bookmarks"))
        .arg(tr("Select some period on timeline and click "
            "with right mouse button on it to create a bookmark."));

    return constrained ? tr("No bookmarks") : kHtmlPlaceholder;
}

QString BookmarkSearchWidget::itemCounterText(int count) const
{
    return tr("%n bookmarks", "", count);
}

} // namespace nx::client::desktop
