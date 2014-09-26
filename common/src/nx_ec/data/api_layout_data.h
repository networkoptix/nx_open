#ifndef EC2_LAYOUT_DATA_H
#define EC2_LAYOUT_DATA_H

#include "api_globals.h"
#include "api_data.h"
#include "api_resource_data.h"

namespace ec2
{

    struct ApiLayoutItemData: ApiData {
        QnUuid id;
        qint32 flags;
        float left;
        float top;
        float right;
        float bottom;
        float rotation;
        QnUuid resourceId;
        QString resourcePath;
        float zoomLeft;
        float zoomTop;
        float zoomRight;
        float zoomBottom;
        QnUuid zoomTargetId;
        QnLatin1Array contrastParams; // TODO: #API I'll think about this one.
        QnLatin1Array dewarpingParams;
    };
#define ApiLayoutItemData_Fields (id)(flags)(left)(top)(right)(bottom)(rotation)(resourceId)(resourcePath) \
                                    (zoomLeft)(zoomTop)(zoomRight)(zoomBottom)(zoomTargetId)(contrastParams)(dewarpingParams)


    struct ApiLayoutItemWithRefData: ApiLayoutItemData {
        QnUuid layoutId;
    };
#define ApiLayoutItemWithRefData_Fields ApiLayoutItemData_Fields (layoutId)


    struct ApiLayoutData : ApiResourceData {
        float cellAspectRatio;
        float horizontalSpacing;
        float verticalSpacing;
        std::vector<ApiLayoutItemData> items;
        bool   editable;
        bool   locked; 
        QString backgroundImageFilename;
        qint32  backgroundWidth;
        qint32  backgroundHeight;
        float backgroundOpacity;
    };
#define ApiLayoutData_Fields ApiResourceData_Fields (cellAspectRatio)(horizontalSpacing)(verticalSpacing)(items)(editable)(locked) \
                                (backgroundImageFilename)(backgroundWidth)(backgroundHeight)(backgroundOpacity)

}

#endif  //EC2_LAYOUT_DATA_H
