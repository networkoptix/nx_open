// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <camera/camera_bookmarks_manager_fwd.h>
#include <core/resource/camera_bookmark.h>
#include <nx/vms/client/desktop/system_context_aware.h>

/**
 * Main interface class to load bookmarks in all common cases.
 */
class QnCameraBookmarksQuery:
    public QObject,
    public nx::vms::client::desktop::SystemContextAware
{
    Q_OBJECT
    using base_type = QObject;

public:
    virtual ~QnCameraBookmarksQuery() override;

    enum State
    {
        /** Query was not executed yet. */
        invalid,

        /** Query will be executed when timeout will pass. */
        queued,

        /** Waiting for the server response. */
        requested,

        /** Part of the data is received. */
        partial,

        /** Cached data is actual. */
        actual,
    };

    /**
     * Unique query id.
     */
    QUuid id() const;

    /** Current query state. */
    State state() const;

    /** Update query state. */
    void setState(State value);

    /**
     * Check if query is valid. Invalid queries cannot be executed. If the query become invalid
     * because of some changes, bookmarksChanged with empty list will be called.
     * @return True if cameras is not empty and filter is valid.
     */
    bool isValid() const;

    /**
     * @return True if query is in active state. Unless query is active it won't be updated
     *     perpetually by the timer event or when the filter changed. Query isn't active by
     *     default.
     */
    bool active() const;

    /**
     * Changes the active state of a query. Actual data will be forcefully updated if query is
     * valid and becomes active.
     */
    void setActive(bool active);

    /**
     * Query loads bookmarks only for the given set of cameras.
     * @return List of cameras.
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
     * @return True if camera actually was added, i.e it wasn't already in the stored set.
     */
    bool addCamera(const QnUuid& value);

    /**
     * Remove camera from the target cameras set. Actual data will be forcefully updated if the
     * new camera set value differs from current.
     * @param value Single camera. If the resulting set is empty, query will become invalid.
     * @return True if camera was really removed, false otherwise.
     */
    bool removeCamera(const QnUuid& value);

    /**
     * Query loads bookmarks filtered by the given filter.
     * @return Current filter.
     */
    QnCameraBookmarkSearchFilter filter() const;

    /**
     * Set filter that will control which bookmarks should be loaded and how. Actual data will
     * be forcefully updated if the new value differs from current.
     * @param value New filter. If it is invalid, the query will become invalid.
     */
    void setFilter(const QnCameraBookmarkSearchFilter& value);

    /**
     * If set, request bookmarks count will be limited to the given value, but if the result size
     * will be equal to the chunk size (which means there are way more bookmarks on the server),
     * then the next chunk request will be sent. Supposed to work only with default ordering, so
     * requests can be specified by the bookmark start time.
     */
    int requestChunkSize() const;

    /**
     * If set, request bookmarks count will be limited to the given value, but if the result size
     * will be equal to the chunk size (which means there are way more bookmarks on the server),
     * then the next chunk request will be sent. Supposed to work only with default ordering, so
     * requests can be specified by the bookmark start time.
     */
    void setRequestChunkSize(int value);

    /**
     * Get locally cached bookmarks.
     */
    const QnCameraBookmarkList& cachedBookmarks() const;

    void setCachedBookmarks(QnCameraBookmarkList value);

    /**
     * Refreshes bookmark data using current parameters. Does nothing if query is not in the active
     * state.
     */
    void refresh();

signals:
    /** Signal to manager that query was changed and needs to be invalidated. */
    void queryChanged();

    void bookmarksChanged(const QnCameraBookmarkList& bookmarks);

private:
    friend class QnCameraBookmarksManagerPrivate;

    QnCameraBookmarksQuery(
        nx::vms::client::desktop::SystemContext* systemContext,
        const QnCameraBookmarkSearchFilter& filter,
        QObject* parent = nullptr);

private:
    const QUuid m_id;
    State m_state = State::invalid;
    QnCameraBookmarkSearchFilter m_filter;
    bool m_active = false;
    int m_requestChunkSize = 0;

    /** Cached bookmarks. */
    QnCameraBookmarkList m_cache;
};
