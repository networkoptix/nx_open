// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <functional>

#include <camera/camera_bookmarks_manager_fwd.h>
#include <core/resource/camera_bookmark.h>
#include <nx/utils/impl_ptr.h>
#include <nx/utils/singleton.h>
#include <nx/vms/event/event_fwd.h>

namespace nx::vms::client::core { class SystemContext; }

class QnCameraBookmarksManagerPrivate;

/** Creates, updates, deletes, fetches and caches bookmarks. */
class NX_VMS_CLIENT_CORE_API QnCameraBookmarksManager: public QObject
{
    Q_OBJECT

public:
    QnCameraBookmarksManager(
        nx::vms::client::core::SystemContext* systemContext,
        QObject* parent = nullptr);

    virtual ~QnCameraBookmarksManager();

     /* Direct API section */

    /// @brief                  Asynchronously gathers bookmarks using specified filter.
    /// @param filter           Filter parameters.
    /// @param callback         Callback for receiving bookmarks data.
    /// @returns                Internal id of the request.
    int getBookmarksAsync(const QnCameraBookmarkSearchFilter& filter, BookmarksCallbackType callback);

    /**
     * @brief Requests bookmarks around specified central point. Calculates tail (new bookmarks
     * over/under specified central point) and body (all other bookmarks).
     * range).
     * Note that the only reasonable sort column is startTime.
     * @param filter Filter parameters (with specified central point)
     * @param source Currently known bookmarks. Used to calculate tail/body values.
     * @param callback Callback for receiving bookmarks data.
     * @return Internal id of the request.
     */
    int getBookmarksAroundPointAsync(
        const QnCameraBookmarkSearchFilter& filter,
        const QnCameraBookmarkList& source,
        BookmarksAroundPointCallbackType&& callback);

    /// @brief                  Add the bookmark to the camera.
    /// @param bookmark         Target bookmark.
    /// @param callback         Callback with operation result.
    void addCameraBookmark(const QnCameraBookmark &bookmark, OperationCallbackType callback = OperationCallbackType());

    /// @brief                  Add the bookmark to the camera and stores record in event log
    ///                         for the specified event.
    /// @param bookmark         Target bookmark.
    /// @param businessRuleId   Business rule identifier for which bookmark is created
    /// @param callback         Callback with operation result.
    void addAcknowledge(
        const QnCameraBookmark &bookmark,
        const QnUuid& eventRuleId,
        OperationCallbackType callback = OperationCallbackType());

    /// @brief                  Update the existing bookmark on the camera.
    /// @param bookmark         Target bookmark.
    /// @param callback         Callback with operation result.
    void updateCameraBookmark(const QnCameraBookmark &bookmark, OperationCallbackType callback = OperationCallbackType());

    /// @brief                  Delete the existing bookmark from the camera.
    /// @param bookmark         Target bookmark id.
    /// @param callback         Callback with operation result.
    void deleteCameraBookmark(const QnUuid &bookmarkId, OperationCallbackType callback = OperationCallbackType());

    /* Queries API section */

    /// @brief                  Instantiate new search query.
    /// @param filter           Filter parameters.
    /// @returns                Search query that is ready to be used.
    QnCameraBookmarksQueryPtr createQuery(const QnCameraBookmarkSearchFilter& filter = {});

    /**
     * Asynchronously execute search query.
     * @param query Target query.
     * @param callback Callback for received bookmarks data. Will be called when data is fully
     *     loaded.
     */
    void sendQueryRequest(
        const QnCameraBookmarksQueryPtr& query,
        BookmarksCallbackType callback = {});

    /**
     * Asynchronously execute search query, requesting data only from the given timepoint.
     * @param query Target query.
     * @param timePoint Minimal start time for the request filter
     * @param callback Callback for received bookmarks data. Will be called when data is fully
     *     loaded.
     */
    void sendQueryTailRequest(
        const QnCameraBookmarksQueryPtr& query,
        std::chrono::milliseconds timePoint,
        BookmarksCallbackType callback = {});

    int getBookmarkTagsAsync(int maxTags, BookmarkTagsCallbackType callback);

signals:
    /* TODO: #dklychkov #2.6 #bookmarks Implement notification transactions bor bookmarks which will call these signals. */
    // So far these signals are emited when the user does something locally.
    /// @brief                  This signal is emitted when new bookmark was added.
    void bookmarkAdded(const QnCameraBookmark &bookmark);
    /// @brief                  This signal is emitted when the bookmark was updated.
    void bookmarkUpdated(const QnCameraBookmark &bookmark);
    /// @brief                  This signal is emitted when the bookmark was removed.
    /// @param                  The removed bookmark GUID.
    void bookmarkRemoved(const QnUuid &bookmarkId);

private:
    nx::utils::ImplPtr<QnCameraBookmarksManagerPrivate> d;
};
