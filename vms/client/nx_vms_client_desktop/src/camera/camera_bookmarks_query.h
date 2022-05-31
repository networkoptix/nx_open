// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <camera/camera_bookmarks_manager_fwd.h>
#include <core/resource/camera_bookmark.h>
#include <nx/vms/client/desktop/system_context_aware.h>
#include <utils/common/from_this_to_shared.h>

/**
 * Main interface class to load bookmarks in all common cases.
 */
class QnCameraBookmarksQuery:
    public QObject,
    public QnFromThisToShared<QnCameraBookmarksQuery>,
    public nx::vms::client::desktop::SystemContextAware
{
    Q_OBJECT
    using base_type = QObject;

public:
    virtual ~QnCameraBookmarksQuery() override;

    /**
     * Unique query id.
     */
    QUuid id() const;

    /**
     * Check if query is valid. Invalid queries cannot be executed. If the query become invalid
     * because of some changes, bookmarksChanged with empty list will be called.
     * @returns True if cameras is not empty and filter is valid.
     */
    bool isValid() const;

    /**
     * Query loads bookmarks only for the given set of cameras.
     * @returns List of cameras.
     */
    const std::set<QnUuid>& cameras() const;

    /**
     * Set cameras for which the bookmarks should be loaded. Actual data will be forcefully
     * updated if the new value differs from current.
     * @param value Set of cameras. If it is empty, query will become invalid.
     */
    void setCameras(const std::set<QnUuid>& value);

    /**
     * Set camera for which the bookmarks should be loaded. Actual data will be forcefully
     * updated if the new camera set value differs from current.
     * @param value Single camera. If it is empty, query will become invalid.
     */
    void setCamera(const QnUuid& value);

    /**
     * Adds camera to the stored set of cameras for which the bookmarks should be loaded.
     * @param value Single camera.
     * @returns True if camera actually was added, i.e it wasn't already in the stored set.
     */
    bool addCamera(const QnUuid& value);

    /**
     * Remove camera from the target cameras set. Actual data will be forcefully updated if the
     * new camera set value differs from current.
     * @param value Single camera. If the resulting set is empty, query will become invalid.
     * @returns True if camera was really removed, false otherwise.
     */
    bool removeCamera(const QnUuid& value);

    /**
     * Query loads bookmarks filtered by the given filter.
     * @returns Current filter.
     */
    QnCameraBookmarkSearchFilter filter() const;

    /**
     * Set filter that will control which bookmarks should be loaded and how. Actual data will
     * be forcefully updated if the new value differs from current.
     * @param value New filter. If it is invalid, the query will become invalid.
     */
    void setFilter(const QnCameraBookmarkSearchFilter& value);

    /**
     * Get locally cached bookmarks.
     */
    QnCameraBookmarkList cachedBookmarks() const;

    /**
     * Asynchronously gathers actual bookmarks from the server.
     * @param callback Callback for receiving bookmarks data.
     */
    void executeRemoteAsync(BookmarksCallbackType callback);

    /**
     * Refreshes bookmark data using current parameters.
     */
    void refresh();

signals:
    void queryChanged();
    void bookmarksChanged(const QnCameraBookmarkList& bookmarks);

private:
    friend class QnCameraBookmarksManagerPrivate;

    QnCameraBookmarksQuery(
        nx::vms::client::desktop::SystemContext* systemContext,
        const QnCameraBookmarkSearchFilter& filter,
        QObject* parent = nullptr);

    QnCameraBookmarksQueryPtr toSharedPointer() const;

private:
    const QUuid m_id;
    QnCameraBookmarkSearchFilter m_filter;
};
