#ifndef QN_LAYOUT_DATA_I_H
#define QN_LAYOUT_DATA_I_H

#include "api_types_i.h"
#include "api_data_i.h"
#include "ec2_resource_data_i.h"

namespace ec2 {

    struct ApiLayoutItemData: ApiData {
        QByteArray uuid;
        qint32 flags;
        float left;
        float top;
        float right;
        float bottom;
        float rotation;
        QnId resourceId;
        QString resourcePath;
        float zoomLeft;
        float zoomTop;
        float zoomRight;
        float zoomBottom;
        QByteArray zoomTargetUuid;
        QByteArray contrastParams;
        QByteArray dewarpingParams;

        QN_DECLARE_STRUCT_SQL_BINDER();
    };

    #define ApiLayoutItemData_Fields (uuid)(flags)(left)(top)(right)(bottom)(rotation)(resourceId)(resourcePath) \
                                    (zoomLeft)(zoomTop)(zoomRight)(zoomBottom)(zoomTargetUuid)(contrastParams)(dewarpingParams)

    struct ApiLayoutData : ApiResourceData {
        float cellAspectRatio;
        float cellSpacingWidth;
        float cellSpacingHeight;
        std::vector<ApiLayoutItemData> items;
        bool   userCanEdit;
        bool   locked;
        QString backgroundImageFilename;
        qint32  backgroundWidth;
        qint32  backgroundHeight;
        float backgroundOpacity;
        qint32 userId;

        QN_DECLARE_STRUCT_SQL_BINDER();
    };


    #define ApiLayoutData_Fields (cellAspectRatio)(cellSpacingWidth)(cellSpacingHeight)(items)(userCanEdit)(locked) \
                                (backgroundImageFilename)(backgroundWidth)(backgroundHeight)(backgroundOpacity) (userId)

} // namespace ec2

#endif // QN_LAYOUT_DATA_I_H