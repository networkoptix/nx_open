
#include "current_layout_bookmarks_watcher.h"

#include <camera/current_layout_bookmarks_cache.h>
#include <ui/workbench/workbench_navigator.h>

namespace
{
    enum { kMaxBookmarksNearThePosition = 512 };

    enum
    {
        kWindowWidthMs = 30000      // 15 seconds before and after current position
        , kLeftOffset = kWindowWidthMs / 2
        , kRightOffset = kLeftOffset

        , kMinWindowChange = kRightOffset / 3
    };

    QnCameraBookmarkList bookmarksAtPosition(const QnCameraBookmarkList &bookmarks
        , qint64 posMs)
    {
        const auto perdicate = [posMs](const QnCameraBookmark &bookmark)
        {
            return (bookmark.startTimeMs <= posMs) && (posMs < bookmark.endTimeMs());
        };

        QnCameraBookmarkList result;
        std::copy_if(bookmarks.cbegin(), bookmarks.cend(), std::back_inserter(result), perdicate);
        return result;
    };

}

//

class QnCurrentLayoutBookmarksWatcher::ListenerHolderPrivate
{
public:
    ListenerHolderPrivate(const Listeners::iterator it
        , QnCurrentLayoutBookmarksWatcher *watcher);

    ~ListenerHolderPrivate();

private:
    const Listeners::iterator m_it;
    QnCurrentLayoutBookmarksWatcher * const m_watcher;
};

QnCurrentLayoutBookmarksWatcher::ListenerHolderPrivate::ListenerHolderPrivate(const Listeners::iterator it
    , QnCurrentLayoutBookmarksWatcher *watcher)
    : m_it(it)
    , m_watcher(watcher)
{
}

QnCurrentLayoutBookmarksWatcher::ListenerHolderPrivate::~ListenerHolderPrivate()
{
    m_watcher->removeListener(m_it);
}

//

QnCurrentLayoutBookmarksWatcher::ListenerData::ListenerData(
    const QnCurrentLayoutBookmarksWatcher::Listener &listenerCallback)
    : listenerCallback(listenerCallback)
    , currentBookmarks()
{}

//

QnCurrentLayoutBookmarksWatcher::QnCurrentLayoutBookmarksWatcher(QObject *parent)
    : base_type(parent)
    , QnWorkbenchContextAware(parent)

    , m_posMs(0)
    , m_bookmarksCache(new QnCurrentLayoutBookmarksCache(kMaxBookmarksNearThePosition
        , Qn::EarliestFirst, kMinWindowChange, parent))
    , m_listeners()
{
    connect(navigator(), &QnWorkbenchNavigator::positionChanged
        , this, &QnCurrentLayoutBookmarksWatcher::updatePosition);
}

QnCurrentLayoutBookmarksWatcher::~QnCurrentLayoutBookmarksWatcher()
{}

QnCurrentLayoutBookmarksWatcher::ListenerLifeHolder QnCurrentLayoutBookmarksWatcher::addListener(
    const QnVirtualCameraResourcePtr &camera
    , const Listener &listenerCallback)
{
    const auto iterator = m_listeners.insertMulti(camera, ListenerData(listenerCallback));
    return ListenerLifeHolder(new ListenerHolderPrivate(iterator, this));
}

void QnCurrentLayoutBookmarksWatcher::onBookmarksChanged(const QnVirtualCameraResourcePtr &camera
    , const QnCameraBookmarkList &bookmarks)
{
    const auto range = m_listeners.equal_range(camera);
    for (auto it = range.first; it != range.second; ++it)
        updateListenerData(it, bookmarks);
}

void QnCurrentLayoutBookmarksWatcher::removeListener(const Listeners::iterator &it)
{
    m_listeners.erase(it);
}

void QnCurrentLayoutBookmarksWatcher::updateListenerData(const Listeners::iterator &it
    , const QnCameraBookmarkList &bookmarks)
{
    auto &listenerData = it.value();
    const auto currentBookmarks = bookmarksAtPosition(bookmarks, m_posMs);
    if (listenerData.currentBookmarks == currentBookmarks)
        return;

    listenerData.currentBookmarks = currentBookmarks;
    listenerData.listenerCallback(currentBookmarks);
}

void QnCurrentLayoutBookmarksWatcher::updatePosition()
{
    enum { kMsInUsec = 1000 };
    const auto posMs = navigator()->positionUsec() / kMsInUsec;
    const QnTimePeriod newWindow = QnTimePeriod::createFromInterval(
        posMs - kLeftOffset, posMs + kRightOffset);

    m_bookmarksCache->setWindow(newWindow);
    m_posMs = posMs;
    for (auto it = m_listeners.begin(); it != m_listeners.end(); ++it)
    {
        const auto camera = it.key();
        updateListenerData(it, m_bookmarksCache->bookmarks(camera));
    }
}

