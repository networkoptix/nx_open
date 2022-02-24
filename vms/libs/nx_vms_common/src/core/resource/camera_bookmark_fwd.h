// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <functional>

#include <QtCore/QList>
#include <QtCore/QSet>
#include <QtCore/QStringList>

struct QnCameraBookmark;
typedef QVector<QnCameraBookmark> QnCameraBookmarkList;
typedef std::vector<QnCameraBookmarkList> QnMultiServerCameraBookmarkList;

struct QnCameraBookmarkSearchFilter;

typedef QSet<QString> QnCameraBookmarkTags;

struct QnCameraBookmarkTag;
typedef QVector<QnCameraBookmarkTag> QnCameraBookmarkTagList;
typedef std::vector<QnCameraBookmarkTagList> QnMultiServerCameraBookmarkTagList;
