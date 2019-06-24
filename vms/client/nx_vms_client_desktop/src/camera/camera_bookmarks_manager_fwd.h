#pragma once

#include <core/resource/resource_fwd.h>
#include <core/resource/camera_bookmark_fwd.h>

class QnCameraBookmarksQuery;
class QnCameraBookmarksManager;
class QnCameraBookmarksManagerPrivate;

using OperationCallbackType = std::function<void (bool success)>;
//typedef std::function<void (bool success, const QnCameraBookmarkList &bookmarks)> BookmarksCallbackType;
using BookmarksCallbackType = std::function<void (bool success, int requestId, const QnCameraBookmarkList &bookmarks)>;
using BookmarkTagsCallbackType = std::function<void (bool, int requestId, QnCameraBookmarkTagList)>;

typedef QnSharedResourcePointer<QnCameraBookmarksQuery> QnCameraBookmarksQueryPtr;
typedef QWeakPointer<QnCameraBookmarksQuery> QnCameraBookmarksQueryWeakPtr;
