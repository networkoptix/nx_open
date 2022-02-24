// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <core/resource/resource_fwd.h>
#include <core/resource/camera_bookmark_fwd.h>

class QnCameraBookmarksQuery;
class QnCameraBookmarksManager;
class QnCameraBookmarksManagerPrivate;

using OperationCallbackType = std::function<void (bool)>;
using BookmarksCallbackType = std::function<void (bool, int, const QnCameraBookmarkList&)>;
using BookmarkTagsCallbackType = std::function<void (bool, int, const QnCameraBookmarkTagList&)>;

typedef QnSharedResourcePointer<QnCameraBookmarksQuery> QnCameraBookmarksQueryPtr;
typedef QWeakPointer<QnCameraBookmarksQuery> QnCameraBookmarksQueryWeakPtr;
