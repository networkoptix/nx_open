// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <vector>

#include <QtCore/QSet>
#include <QtCore/QString>
#include <QtCore/QVector>

namespace nx::vms::common {

struct CameraBookmark;
struct CameraBookmarkSearchFilter;
struct CameraBookmarkTag;

} // namespace nx::vms::common

using QnCameraBookmark = nx::vms::common::CameraBookmark;
using QnCameraBookmarkSearchFilter = nx::vms::common::CameraBookmarkSearchFilter;
using QnCameraBookmarkTag =  nx::vms::common::CameraBookmarkTag;

typedef QVector<QnCameraBookmark> QnCameraBookmarkList;
typedef std::vector<QnCameraBookmarkList> QnMultiServerCameraBookmarkList;

typedef QSet<QString> QnCameraBookmarkTags;

typedef QVector<QnCameraBookmarkTag> QnCameraBookmarkTagList;
typedef std::vector<QnCameraBookmarkTagList> QnMultiServerCameraBookmarkTagList;
