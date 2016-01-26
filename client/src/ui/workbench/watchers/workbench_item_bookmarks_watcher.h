
#pragma once

#include <QtCore/QObject>

#include <client/client_color_types.h>
#include <core/resource/resource_fwd.h>
#include <core/resource/camera_bookmark_fwd.h>
#include <ui/workbench/workbench_context_aware.h>
#include <utils/common/connective.h>
#include <ui/customization/customized.h>

class QnUuid;
class QnWorkbenchBookmarksCache;
class QnWorkbenchItem;
class QnTimelineBookmarksWatcher;
class QnCameraBookmarkAggregation;

// @brief Loads bookmarks for all items on current layout in vicinity of current position.
// Updates current visible bookmarks on overlays for each item.
class QnWorkbenchItemBookmarksWatcher : public Customized< Connective<QObject> >
    , public QnWorkbenchContextAware
{
    Q_OBJECT

    Q_PROPERTY(QnBookmarkColors bookmarkColors READ bookmarkColors WRITE setBookmarkColors)

    typedef Customized< Connective<QObject> > base_type;

public:
    QnWorkbenchItemBookmarksWatcher(QObject *parent);

    virtual ~QnWorkbenchItemBookmarksWatcher();

    QnBookmarkColors bookmarkColors() const;

    void setBookmarkColors(const QnBookmarkColors &colors);

private:
    void onItemAdded(QnWorkbenchItem *item);

    void onItemRemoved(QnWorkbenchItem *item);

    void updatePosition(qint64 position);

    void onBookmarksChanged(const QnVirtualCameraResourcePtr &camera
        , const QnCameraBookmarkList &bookmarks);

private:
    class ListenerHolderPrivate;
    typedef QSharedPointer<ListenerHolderPrivate> ListenerLifeHolder;
    typedef std::function<void (const QnCameraBookmarkList &bookmarks)> Listener;

    struct ListenerDataPrivate;
    typedef QSharedPointer<ListenerDataPrivate> ListenerDataPrivatePtr;
    typedef QMultiMap<QnVirtualCameraResourcePtr, ListenerDataPrivatePtr> Listeners;

    ListenerLifeHolder addListener(const QnVirtualCameraResourcePtr &camera
        , const Listener &listenerCallback);

    void removeListener(const Listeners::iterator &it);

    void updateListenerData(const Listeners::iterator &it
        , const QnCameraBookmarkList &bookmarks);

private:
    typedef QScopedPointer<QnCameraBookmarkAggregation> QnCameraBookmarkAggregationPtr;
    typedef QScopedPointer<QnWorkbenchBookmarksCache> QnCurrentLayoutBookmarksCachePtr;
    typedef QMap<QnWorkbenchItem *, ListenerLifeHolder> ItemListenersMap;

    const QnCameraBookmarkAggregationPtr m_aggregationHelper;
    QnTimelineBookmarksWatcher * const m_timelineWatcher;
    QnCurrentLayoutBookmarksCachePtr m_bookmarksCache;
    ItemListenersMap m_itemListinersMap;
    Listeners m_listeners;
    qint64 m_posMs;

    QnBookmarkColors m_colors;
};

