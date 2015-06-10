#ifndef QN_CAMERA_BOOKMARK_FWD_H
#define QN_CAMERA_BOOKMARK_FWD_H

#include <functional>

#include <QtCore/QList>

#include <utils/common/model_functions_fwd.h>

struct QnCameraBookmark;
typedef QList<QnCameraBookmark> QnCameraBookmarkList;

struct QnCameraBookmarkSearchFilter;

typedef QStringList QnCameraBookmarkTags;
typedef std::function<void (bool success, const QnCameraBookmarkList &bookmarks)> BookmarksCallbackType;

QN_FUSION_DECLARE_FUNCTIONS(QnCameraBookmark, (sql_record)(json)(eq))
QN_FUSION_DECLARE_FUNCTIONS(QnCameraBookmarkSearchFilter, (json)(eq))

#endif // QN_CAMERA_BOOKMARK_FWD_H

