#pragma once

#include <core/resource/resource_fwd.h>
#include <core/resource/camera_bookmark_fwd.h>

class QnCameraBookmarksQuery;
class QnCameraBookmarksManager;
class QnCameraBookmarksManagerPrivate;

typedef std::function<void (bool success)> OperationCallbackType;
typedef std::function<void (bool success, const QnCameraBookmarkList &bookmarks)> BookmarksCallbackType;
typedef std::function<void (bool success, const QnCameraBookmarkList &bookmarks, int requestId)> BookmarksInternalCallbackType;

typedef QnSharedResourcePointer<QnCameraBookmarksQuery> QnCameraBookmarksQueryPtr;
typedef QWeakPointer<QnCameraBookmarksQuery> QnCameraBookmarksQueryWeakPtr;
