#ifndef EC2_LAYOUT_DATA_H
#define EC2_LAYOUT_DATA_H

#include "api_globals.h"
#include "api_data.h"
#include "api_resource_data.h"

namespace ec2
{

    struct ApiLayoutItemData: ApiData {
        QnLatin1Array uuid; // TODO: #API rename to 'id'.
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
        QnLatin1Array zoomTargetUuid; // TODO: #API rename 'zoomTargetId'
        QnLatin1Array contrastParams; // TODO: #API I'll think about this one.
        QnLatin1Array dewarpingParams;
    };
#define ApiLayoutItemData_Fields (uuid)(flags)(left)(top)(right)(bottom)(rotation)(resourceId)(resourcePath) \
                                    (zoomLeft)(zoomTop)(zoomRight)(zoomBottom)(zoomTargetUuid)(contrastParams)(dewarpingParams)


    struct ApiLayoutItemWithRefData: ApiLayoutItemData {
        QnId layoutId;
    };
#define ApiLayoutItemWithRefData_Fields ApiLayoutItemData_Fields (layoutId)


    struct ApiLayoutData : ApiResourceData {
        float cellAspectRatio;
        float cellSpacingWidth; // TODO: #API rename 'horizontalSpacing'
        float cellSpacingHeight; // TODO: #API rename 'verticalSpacing'
        std::vector<ApiLayoutItemData> items;
        bool   userCanEdit; // TODO: #API rename 'editable'
        bool   locked; 
        QString backgroundImageFilename;
        qint32  backgroundWidth;
        qint32  backgroundHeight;
        float backgroundOpacity;
        qint32 userId; // TODO: #API what is this? It certainly doesn't belong to public API. Remove?
    };
#define ApiLayoutData_Fields ApiResourceData_Fields (cellAspectRatio)(cellSpacingWidth)(cellSpacingHeight)(items)(userCanEdit)(locked) \
                                (backgroundImageFilename)(backgroundWidth)(backgroundHeight)(backgroundOpacity) (userId)

}

#endif  //EC2_LAYOUT_DATA_H
