#pragma once

#include <functional>

#include <core/resource/resource_fwd.h>
#include <core/resource/camera_bookmark_fwd.h>

#include <recording/time_period.h>

class QnBookmarksLoader;

class QnCameraBookmarksManagerPrivate;

class QnCameraBookmarksManager : public QObject
{
    Q_OBJECT
public:
    QnCameraBookmarksManager(QObject *parent = nullptr);

    virtual ~QnCameraBookmarksManager();

    /// @brief Asynchronously gathers bookmarks using specified filter
    /// @param filter Filter parameters
    /// @param callback Callback for receiving bookmarks data
    void getBookmarksAsync(const QnVirtualCameraResourceList &cameras, const QnCameraBookmarkSearchFilter &filter, const QUuid &requestId, BookmarksCallbackType callback);

    void loadBookmarks(const QnVirtualCameraResourcePtr &camera, const QnTimePeriod &period);   

    QnCameraBookmarkList bookmarks(const QnVirtualCameraResourcePtr &camera) const;    
protected:
    Q_DECLARE_PRIVATE(QnCameraBookmarksManager);

signals:
    void bookmarksChanged(const QnVirtualCameraResourcePtr &camera);
private:
    QScopedPointer<QnCameraBookmarksManagerPrivate> d_ptr;
};
