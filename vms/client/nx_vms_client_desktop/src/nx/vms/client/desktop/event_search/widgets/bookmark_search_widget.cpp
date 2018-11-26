#include "bookmark_search_widget.h"

#include <core/resource/camera_resource.h>
#include <ui/style/skin.h>
#include <ui/workbench/workbench_context.h>
#include <ui/workbench/workbench_navigator.h>

#include <nx/vms/client/desktop/event_search/models/bookmark_search_list_model.h>
#include <nx/utils/log/assert.h>

namespace nx::vms::client::desktop {

BookmarkSearchWidget::BookmarkSearchWidget(QnWorkbenchContext* context, QWidget* parent):
    base_type(context, new BookmarkSearchListModel(context), parent)
{
    setPlaceholderPixmap(qnSkin->pixmap("events/placeholders/bookmarks.png"));

    connect(this, &AbstractSearchWidget::textFilterChanged,
        [this](const QString& text)
        {
            auto bookmarksModel = qobject_cast<BookmarkSearchListModel*>(model());
            NX_CRITICAL(bookmarksModel);
            bookmarksModel->setFilterText(text);
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

} // namespace nx::vms::client::desktop
