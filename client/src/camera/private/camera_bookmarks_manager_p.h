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
    QnCameraBookmarksManagerPrivate(QObject *parent);
    
    virtual ~QnCameraBookmarksManagerPrivate();

    void getBookmarksAsync(const QnVirtualCameraResourceList &cameras, const QnCameraBookmarkSearchFilter &filter, const QUuid &requestId, BookmarksCallbackType callback);

    void loadBookmarks(const QnVirtualCameraResourcePtr &camera, const QnTimePeriod &period);

    QnCameraBookmarkList bookmarks(const QnVirtualCameraResourcePtr &camera) const;

    void clearCache();
private:
    /// @brief Callback for bookmarks. If data is nullptr it means error occurred
    void bookmarksDataEvent(const QnCameraBookmarkList &bookmarks);

    QnBookmarksLoader *loader(const QnVirtualCameraResourcePtr &camera, bool createIfNotExists = true);

private:
    Q_DECLARE_PUBLIC(QnCameraBookmarksManager)
    QnCameraBookmarksManager *q_ptr;

    typedef int HandleType;
    typedef std::map<const QnBookmarksLoader *, HandleType> AnswersContainer;

    /// @brief Structure holds data for the request of the bookmarks
    struct BookmarkRequestHolder
    {
        QnTimePeriod targetPeriod;
        BookmarksCallbackType callback;
        QnCameraBookmarkList bookmarks;
        AnswersContainer answers;
        bool success;
    };

    typedef QList<BookmarkRequestHolder> RequestsContainer;
    typedef QHash<QnVirtualCameraResourcePtr, QnBookmarksLoader *> LoadersContainer;

    LoadersContainer m_loaderByResource;
    RequestsContainer m_requests;

    
};