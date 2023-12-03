// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <vector>

#include <QtCore/QSet>
#include <QtCore/QString>
#include <QtCore/QVector>

struct QnCameraBookmark;
typedef std::vector<QnCameraBookmark> QnCameraBookmarkList;
typedef std::vector<QnCameraBookmarkList> QnMultiServerCameraBookmarkList;

struct QnCameraBookmarkSearchFilter;

typedef QSet<QString> QnCameraBookmarkTags;

struct QnCameraBookmarkTag;
typedef QVector<QnCameraBookmarkTag> QnCameraBookmarkTagList;
typedef std::vector<QnCameraBookmarkTagList> QnMultiServerCameraBookmarkTagList;
