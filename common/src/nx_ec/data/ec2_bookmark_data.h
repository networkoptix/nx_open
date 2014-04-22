#ifndef __API_BOOKMARK_DATA_H_
#define __API_BOOKMARK_DATA_H_

#include "api_data.h"

#include <core/resource/camera_bookmark.h>
#include <utils/common/id.h>

namespace ec2
{
    struct ApiCameraBookmark;
    
    #include "ec2_bookmark_data_i.h"
    
    struct ApiCameraBookmark: ApiCameraBookmarkData {
        void toBookmark(QnCameraBookmark& bookmark) const;
        void fromBookmark(const QnCameraBookmark& bookmark);
        QN_DECLARE_STRUCT_SQL_BINDER();
    };
    QN_DEFINE_STRUCT_SQL_BINDER(ApiCameraBookmark, ApiCameraBookmarkFields);
    struct ApiCameraBookmarkWithRef: ApiCameraBookmark { QnId cameraId; };

    struct ApiCameraBookmarkTag {
        QnId bookmarkGuid; 
        QString name;
        QN_DECLARE_STRUCT_SQL_BINDER();
    };
    #define ApiCameraBookmarkTagFields (bookmarkGuid) (name)
    QN_DEFINE_STRUCT_SQL_BINDER(ApiCameraBookmarkTag, ApiCameraBookmarkTagFields);

    typedef QnCameraBookmarkTagsUsage ApiCameraBookmarkTagsUsage;
}

#endif __API_BOOKMARK_DATA_H_