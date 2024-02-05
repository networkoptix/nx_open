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

using CameraBookmarkList = QVector<CameraBookmark>;
using MultiServerCameraBookmarkList = std::vector<CameraBookmarkList>;
using CameraBookmarkTags = QSet<QString>;
using CameraBookmarkTagList = QVector<CameraBookmarkTag>;
using MultiServerCameraBookmarkTagList = std::vector<CameraBookmarkTagList>;

} // namespace nx::vms::common

// Delete these when all instances in the code have been replaced with the above.
using QnCameraBookmark = nx::vms::common::CameraBookmark;
using QnCameraBookmarkSearchFilter = nx::vms::common::CameraBookmarkSearchFilter;
using QnCameraBookmarkTag =  nx::vms::common::CameraBookmarkTag;
using QnCameraBookmarkList = nx::vms::common::CameraBookmarkList;
using QnMultiServerCameraBookmarkList = nx::vms::common::MultiServerCameraBookmarkList;
using QnCameraBookmarkTags = nx::vms::common::CameraBookmarkTags;
using QnCameraBookmarkTagList = nx::vms::common::CameraBookmarkTagList;
using QnMultiServerCameraBookmarkTagList = nx::vms::common::MultiServerCameraBookmarkTagList;
