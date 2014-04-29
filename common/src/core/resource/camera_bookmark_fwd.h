#ifndef QN_CAMERA_BOOKMARK_FWD_H
#define QN_CAMERA_BOOKMARK_FWD_H

#include <QtCore/QList>

#include <utils/common/model_functions_fwd.h>

struct QnCameraBookmark;
typedef QList<QnCameraBookmark> QnCameraBookmarkList;

struct QnCameraBookmarkSearchFilter;

typedef QStringList QnCameraBookmarkTagsUsage;

QN_FUSION_DECLARE_FUNCTIONS(QnCameraBookmark, (sql)(json)(eq))
QN_FUSION_DECLARE_FUNCTIONS(QnCameraBookmarkSearchFilter, (json)(eq))

#endif // QN_CAMERA_BOOKMARK_FWD_H

