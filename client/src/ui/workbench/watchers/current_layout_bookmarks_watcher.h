
#pragma once

#include <QtCore/QObject>

#include <core/resource/resource_fwd.h>
#include <core/resource/camera_bookmark_fwd.h>
#include <ui/workbench/workbench_context_aware.h>
#include <utils/common/connective.h>

class QnUuid;
class QnCurrentLayoutBookmarksCache;

class QnCurrentLayoutBookmarksWatcher : public Connective<QObject>
    , public QnWorkbenchContextAware
{
    Q_OBJECT

    typedef Connective<QObject> base_type;

public:
    QnCurrentLayoutBookmarksWatcher(QObject *parent);

    virtual ~QnCurrentLayoutBookmarksWatcher();

    class ListenerHolderPrivate;
    typedef QSharedPointer<ListenerHolderPrivate> ListenerLifeHolder;
    typedef std::function<void (const QnCameraBookmarkList &bookmarks)> Listener;

    ListenerLifeHolder addListener(const QnVirtualCameraResourcePtr &camera
        , const Listener &listenerCallback);

private:
    void updatePosition();

    void onBookmarksChanged(const QnVirtualCameraResourcePtr &camera
        , const QnCameraBookmarkList &bookmarks);

private:
    struct ListenerData
    {
        Listener listenerCallback;
        QnCameraBookmarkList currentBookmarks;

        explicit ListenerData(const Listener &listenerCallback);
    };

    typedef QMultiMap<QnVirtualCameraResourcePtr, ListenerData> Listeners;
    void removeListener(const Listeners::iterator &it);

    void updateListenerData(const Listeners::iterator &it
        , const QnCameraBookmarkList &bookmarks);

private:
    typedef QScopedPointer<QnCurrentLayoutBookmarksCache> QnCurrentLayoutBookmarksCachePtr;

    qint64 m_posMs;
    QnCurrentLayoutBookmarksCachePtr m_bookmarksCache;
    Listeners m_listeners;
};

