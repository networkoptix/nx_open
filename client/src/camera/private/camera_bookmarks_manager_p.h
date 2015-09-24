#pragma once

#include <functional>

#include <core/resource/resource_fwd.h>
#include <core/resource/camera_bookmark_fwd.h>

#include <recording/time_period.h>

class QnBookmarksLoader;
class QnCameraBookmarksManager;

class QnCameraBookmarksManagerPrivate : public QObject {
    Q_OBJECT
public:
    QnCameraBookmarksManagerPrivate(QnCameraBookmarksManager *parent);
    
    virtual ~QnCameraBookmarksManagerPrivate();

    void getBookmarksAsync(const QnVirtualCameraResourceList &cameras, const QnCameraBookmarkSearchFilter &filter, const QUuid &requestId, BookmarksCallbackType callback);

    void loadBookmarks(const QnVirtualCameraResourcePtr &camera, const QnTimePeriod &period);

    QnCameraBookmarkList bookmarks(const QnVirtualCameraResourcePtr &camera) const;

    void clearCache();
private:
    /// @brief Callback for bookmarks. If data is nullptr it means error occurred
    void bookmarksDataEvent(const QnCameraBookmarkList &bookmarks);

private slots:
    void handleDataLoaded(int status, const QnCameraBookmarkList &bookmarks, int handle);

private:
    Q_DECLARE_PUBLIC(QnCameraBookmarksManager)
    QnCameraBookmarksManager *q_ptr;

    struct RequestInfo {
        int handle;
        BookmarksCallbackType callback;
    };
    QHash<QUuid, RequestInfo> m_requests;
    
};