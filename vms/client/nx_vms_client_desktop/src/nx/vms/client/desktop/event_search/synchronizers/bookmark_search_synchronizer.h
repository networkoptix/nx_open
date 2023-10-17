// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <QtCore/QPointer>

#include "abstract_search_synchronizer.h"

class QAction;

namespace nx::vms::client::desktop {

class CommonObjectSearchSetup;

/**
 * An utility class to synchronize Right Panel bookmark tab state
 * with bookmarks display on the timeline.
 */
class BookmarkSearchSynchronizer: public AbstractSearchSynchronizer
{
public:
    BookmarkSearchSynchronizer(
        WindowContext* context,
        CommonObjectSearchSetup* searchSetup,
        QObject* parent = nullptr);

private:
    QAction* bookmarksAction() const;
    void updateTimelineBookmarks();

private:
    const QPointer<CommonObjectSearchSetup> m_searchSetup;
};

} // namespace nx::vms::client::desktop
