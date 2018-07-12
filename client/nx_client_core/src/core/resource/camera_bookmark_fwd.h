#pragma once

#include <functional>

#include <QtCore/QList>
#include <QtCore/QStringList>

struct QnCameraBookmark;
typedef QVector<QnCameraBookmark> QnCameraBookmarkList;
typedef std::vector<QnCameraBookmarkList> QnMultiServerCameraBookmarkList;

struct QnCameraBookmarkSearchFilter;

typedef QSet<QString> QnCameraBookmarkTags;

struct QnCameraBookmarkTag;
typedef QVector<QnCameraBookmarkTag> QnCameraBookmarkTagList;
typedef std::vector<QnCameraBookmarkTagList> QnMultiServerCameraBookmarkTagList;
