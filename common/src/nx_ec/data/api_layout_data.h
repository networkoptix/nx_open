#ifndef EC2_LAYOUT_DATA_H
#define EC2_LAYOUT_DATA_H

#include "api_globals.h"
#include "api_data.h"
#include "api_resource_data.h"

namespace ec2
{

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
        QByteArray contrastParams; // TODO: #Elric #EC2 rename
        QByteArray dewarpingParams;
    };
#define ApiLayoutItemData_Fields (uuid)(flags)(left)(top)(right)(bottom)(rotation)(resourceId)(resourcePath) \
                                    (zoomLeft)(zoomTop)(zoomRight)(zoomBottom)(zoomTargetUuid)(contrastParams)(dewarpingParams)


    struct ApiLayoutItemWithRefData: ApiLayoutItemData {
        QnId layoutId;
    };
#define ApiLayoutItemWithRefData_Fields ApiLayoutItemData_Fields (layoutId)


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
    };
#define ApiLayoutData_Fields ApiResourceData_Fields (cellAspectRatio)(cellSpacingWidth)(cellSpacingHeight)(items)(userCanEdit)(locked) \
                                (backgroundImageFilename)(backgroundWidth)(backgroundHeight)(backgroundOpacity) (userId)

}

#endif  //EC2_LAYOUT_DATA_H
