#pragma once

#include <QtCore/QPointer>

#include "abstract_search_synchronizer.h"

class QAction;

namespace nx::vms::client::desktop {

class BookmarkSearchWidget;

/**
 * An utility class to synchronize Right Panel bookmark tab state
 * with bookmarks display on the timeline.
 */
class BookmarkSearchSynchronizer: public AbstractSearchSynchronizer
{
public:
    BookmarkSearchSynchronizer(QnWorkbenchContext* context,
        BookmarkSearchWidget* bookmarkSearchWidget, QObject* parent = nullptr);

private:
    QAction* bookmarksAction() const;
    void updateTimelineBookmarks();

private:
    const QPointer<BookmarkSearchWidget> m_bookmarkSearchWidget;
};

} // namespace nx::vms::client::desktop
