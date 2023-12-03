// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <api/server_rest_connection_fwd.h>
#include <core/resource/camera_bookmark_fwd.h>
#include <nx/vms/client/core/event_search/models/fetched_data.h>
#include <nx/vms/client/core/resource/resource_fwd.h>

class QnCameraBookmarksQuery;
class QnCameraBookmarksManager;
class QnCameraBookmarksManagerPrivate;

using OperationCallbackType = std::function<void (bool)>;
using BookmarksCallbackType = std::function<void (bool, rest::Handle, QnCameraBookmarkList)>;
using BookmarkTagsCallbackType = std::function<void (bool, rest::Handle, QnCameraBookmarkTagList)>;

typedef QSharedPointer<QnCameraBookmarksQuery> QnCameraBookmarksQueryPtr;
typedef QWeakPointer<QnCameraBookmarksQuery> QnCameraBookmarksQueryWeakPtr;

using BookmarkFetchedData = nx::vms::client::core::FetchedData<QnCameraBookmarkList>;
using BookmarksAroundPointCallbackType = std::function<void (
    bool success,
    int requestId,
    BookmarkFetchedData&& data)>;
