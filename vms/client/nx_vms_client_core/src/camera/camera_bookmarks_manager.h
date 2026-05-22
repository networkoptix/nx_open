// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <camera/camera_bookmarks_manager_fwd.h>
#include <core/resource/camera_bookmark.h>
#include <nx/utils/async_handler_executor.h>
#include <nx/utils/impl_ptr.h>
#include <nx/vms/event/event_fwd.h>

namespace nx::vms::client::core { class SystemContext; }

class QnCameraBookmarksManagerPrivate;

/** Creates, updates, deletes, fetches and caches bookmarks. */
class NX_VMS_CLIENT_CORE_API QnCameraBookmarksManager: public QObject
{
    Q_OBJECT
    using base_type = QObject;

public:
    QnCameraBookmarksManager(
        nx::vms::client::core::SystemContext* systemContext,
        QObject* parent = nullptr);

    virtual ~QnCameraBookmarksManager() override;

    // Direct API section.

    /**
     * Asynchronously requests bookmarks using specified filter.
     * @param filter Filter parameters.
     * @param callback Callback for receiving bookmarks data.
     * @param executor Callback execution thread control object.
     * @return Internal id of the request.
     */
    int getBookmarksAsync(const QnCameraBookmarkSearchFilter& filter,
        BookmarksCallbackType callback,
        nx::utils::AsyncHandlerExecutor executor);

    /**
     * Requests bookmarks around specified central point. Calculates tail (new bookmarks over/under
     * specified central point) and body (all other bookmarks) ranges.
     * Note that the only reasonable sort column is startTime.
     * @param filter Filter parameters (with specified central point).
     * @param source Currently known bookmarks. Used to calculate tail/body values.
     * @param callback Callback for receiving bookmarks data.
     * @return Internal id of the request.
     */
    int getBookmarksAroundPointAsync(
        const QnCameraBookmarkSearchFilter& filter,
        const QnCameraBookmarkList& source,
        BookmarksAroundPointCallbackType&& callback);

    enum class BookmarkOperation
    {
        create,
        update,
        remove,
    };

    /**
     * Creates, updates or removes bookmark. Returns the added or modified bookmark, along with the
     * operation success result via the callback. If the operation succeeds, updates the caches of
     * the registered queries.
     * @param operation Specifies desired operation.
     * @param bookmark The bookmark on which the operation is expected to be performed. A valid
     *     bookmark is expected for any operation.
     * @param callback The callback through which the operation result will be returned.
     * @return true if valid arguments were provided, no issues occurred while preparing and
     *     sending request. Returns false otherwise. Note that if false is returned, the provided
     *     callback will never be called.
     */
    bool submitBookmarkOperation(BookmarkOperation operation,
        const QnCameraBookmark& bookmark,
        OperationCallbackType&& callback = OperationCallbackType());

    /**
     * Updates caches of the registered queries for the case when a bookmark was added externally,
     * without using bookmark manager.
     */
    void addExistingBookmark(const QnCameraBookmark& bookmark);

    // Queries API section.

    /**
     * Instantiate new search query.
     * @param filter Filter parameters.
     * @return Search query that is ready to be used.
     */
    QnCameraBookmarksQueryPtr createQuery(const QnCameraBookmarkSearchFilter& filter = {});

    /**
     * Asynchronously execute search query.
     * @param query Target query.
     * @param callback Callback for received bookmarks data. Will be called when data is fully
     *     loaded.
     * @return Request ID.
     */
    rest::Handle sendQueryRequest(
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
    /**
     * Signal is emitted when new bookmark was successfully added via bookmarks manager, or when
     * valid externally added bookmark was passed to the addExistingBookmark method.
     */
    void bookmarkAdded(const QnCameraBookmark& bookmark);

    /** Signal is emitted when the bookmark was successfully updated via bookmarks manager. */
    void bookmarkUpdated(const QnCameraBookmark& bookmark);

    /** Signal is emitted when the bookmark was successfully removed via bookmarks manager. */
    void bookmarkRemoved(const nx::Uuid& bookmarkId);

private:
    nx::utils::ImplPtr<QnCameraBookmarksManagerPrivate> d;
};
