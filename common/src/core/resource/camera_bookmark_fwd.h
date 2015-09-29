#pragma once

#include <functional>

#include <QtCore/QList>
#include <QtCore/QStringList>

struct QnCameraBookmark;
typedef QVector<QnCameraBookmark> QnCameraBookmarkList;
typedef std::vector<QnCameraBookmarkList> MultiServerCameraBookmarkList;

struct QnCameraBookmarkSearchFilter;

typedef QSet<QString> QnCameraBookmarkTags;
