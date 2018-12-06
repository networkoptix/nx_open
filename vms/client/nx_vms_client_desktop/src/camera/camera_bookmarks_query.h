#pragma once

#include <camera/camera_bookmarks_manager_fwd.h>
#include <core/resource/camera_bookmark.h>

#include <utils/common/from_this_to_shared.h>

/** Main interface class to load bookmarks in all common cases. */
class QnCameraBookmarksQuery: public QObject, public QnFromThisToShared<QnCameraBookmarksQuery> {
    Q_OBJECT

    typedef QObject base_type;
public:
    virtual ~QnCameraBookmarksQuery();

    /// @brief                  Unique query id.
    QUuid id() const;

    /// @brief                  Check if query is valid. Invalid queries cannot be executed.
    ///                         If the query become invalid because of some changes, bookmarksChanged with empty list will be called.
    /// @returns                True if cameras is not empty and filter is valid.
    bool isValid() const;

    /// @brief                  Query loads bookmarks only for the given set of cameras.
    /// @returns                List of cameras.
    QnVirtualCameraResourceSet cameras() const;

    /// @brief                  Set cameras for which the bookmarks should be loaded.
    ///                         Actual data will be forcefully updated if the new value differs from current.
    /// @param value            Set of cameras. If it is empty, query will become invalid.
    void setCameras(const QnVirtualCameraResourceSet &value);

    /// @brief                  Set camera for which the bookmarks should be loaded.
    ///                         Actual data will be forcefully updated if the new camera set value differs from current.
    /// @param value            Single camera. If it is empty, query will become invalid.
    void setCamera(const QnVirtualCameraResourcePtr &value);

    /// @brief                  Remove camera from the target cameras set.
    ///                         Actual data will be forcefully updated if the new camera set value differs from current.
    /// @param value            Single camera. If the resulting set is empty, query will become invalid.
    /// @returns                True if camera was really removed, false otherwise.
    bool removeCamera(const QnVirtualCameraResourcePtr &value);

    /// @brief                  Query loads bookmarks filtered by the given filter.
    /// @returns                Current filter.
    QnCameraBookmarkSearchFilter filter() const;

    /// @brief                  Set filter that will control which bookmarks should be loaded and how.
    ///                         Actual data will be forcefully updated if the new value differs from current.
    /// @param value            New filter. If it is invalid, the query will become invalid.
    void setFilter(const QnCameraBookmarkSearchFilter &value);

    /// @brief                  Get locally cached bookmarks.
    QnCameraBookmarkList cachedBookmarks() const;

    /// @brief                  Asynchronously gathers actual bookmarks from the server.
    /// @param callback         Callback for receiving bookmarks data.
    void executeRemoteAsync(BookmarksCallbackType callback);

    /// @brief                  Refreshes bookmark data using current parameters
    void refresh();

signals:
    void queryChanged();
    void bookmarksChanged(const QnCameraBookmarkList &bookmarks);

private:
    friend class QnCameraBookmarksManagerPrivate;
    QnCameraBookmarksQuery(const QnVirtualCameraResourceSet &cameras, const QnCameraBookmarkSearchFilter &filter, QObject *parent = nullptr);
    QnCameraBookmarksQueryPtr toSharedPointer() const;

private:
    const QUuid m_id;
    QnVirtualCameraResourceSet m_cameras;
    QnCameraBookmarkSearchFilter m_filter;
};
