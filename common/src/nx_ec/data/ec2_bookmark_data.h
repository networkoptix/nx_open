#ifndef __API_BOOKMARK_DATA_H_
#define __API_BOOKMARK_DATA_H_

#include "api_data.h"

#include <core/resource/camera_bookmark.h>
#include <utils/common/id.h>

namespace ec2
{
    #include "ec2_bookmark_data_i.h"
    
    void fromApiToBookmark(const ApiCameraBookmarkData &data, QnCameraBookmark& bookmark);
    void fromBookmarkToApi(const QnCameraBookmark &bookmark, ApiCameraBookmarkData &data);

    QN_DEFINE_STRUCT_SQL_BINDER(ApiCameraBookmarkData, ApiCameraBookmarkFields);
    struct ApiCameraBookmarkWithRef: ApiCameraBookmarkData { QnId cameraId; };

    QN_DEFINE_STRUCT_SQL_BINDER(ApiCameraBookmarkTag, ApiCameraBookmarkTagFields);

    typedef QnCameraBookmarkTagsUsage ApiCameraBookmarkTagsUsage;
}

#endif __API_BOOKMARK_DATA_H_