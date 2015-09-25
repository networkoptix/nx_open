#pragma once

#include <camera/camera_bookmarks_manager_fwd.h>
#include <core/resource/camera_bookmark.h>

#include <utils/common/from_this_to_shared.h>

class QnCameraBookmarksQuery: public QObject, public QnFromThisToShared<QnCameraBookmarksQuery> {
    Q_OBJECT

    typedef QObject base_type;
public:
    QnCameraBookmarksQuery(QObject *parent = nullptr);
    QnCameraBookmarksQuery(const QnVirtualCameraResourceList &cameras, const QnCameraBookmarkSearchFilter &filter, QObject *parent = nullptr);
    virtual ~QnCameraBookmarksQuery();

    bool autoUpdate() const;
    void setAutoUpdate(bool value);

    QnVirtualCameraResourceList cameras() const;
    void setCameras(const QnVirtualCameraResourceList &value);

    QnCameraBookmarkSearchFilter filter() const;
    void setFilter(const QnCameraBookmarkSearchFilter &value);

    QnCameraBookmarkList executeLocal() const;
    void executeRemoteAsync(BookmarksCallbackType callback);

signals:
    void queryChanged();
    void bookmarksChanged(const QnCameraBookmarkList &bookmarks);

private:
    QnCameraBookmarksQueryPtr toSharedPointer() const;

private:
    bool m_autoUpdate;
    QnVirtualCameraResourceList m_cameras;
    QnCameraBookmarkSearchFilter m_filter;
};
