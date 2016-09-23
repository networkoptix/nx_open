#ifndef EC2_LAYOUT_DATA_H
#define EC2_LAYOUT_DATA_H

#include "api_globals.h"
#include "api_data.h"
#include "api_resource_data.h"
#include <core/resource/resource_type.h>

namespace ec2
{

    struct ApiLayoutItemData: ApiIdData
    {
        ApiLayoutItemData();

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
        bool displayInfo;                   /**< Should info be displayed on the item. */
    };
#define ApiLayoutItemData_Fields ApiIdData_Fields (flags)(left)(top)(right)(bottom)(rotation)(resourceId)(resourcePath) \
                                    (zoomLeft)(zoomTop)(zoomRight)(zoomBottom)(zoomTargetId)(contrastParams)(dewarpingParams) \
                                    (displayInfo)


    struct ApiLayoutItemWithRefData: ApiLayoutItemData {
        QnUuid layoutId;
    };
#define ApiLayoutItemWithRefData_Fields ApiLayoutItemData_Fields (layoutId)


    struct ApiLayoutData: ApiResourceData
    {
        ApiLayoutData()
        {
            typeId = QnResourceTypePool::kLayoutTypeUuid;
        }

        float cellAspectRatio;
        float horizontalSpacing;
        float verticalSpacing;
        std::vector<ApiLayoutItemData> items;
        bool   locked;
        QString backgroundImageFilename;
        qint32  backgroundWidth;
        qint32  backgroundHeight;
        float backgroundOpacity;
    };
#define ApiLayoutData_Fields ApiResourceData_Fields (cellAspectRatio)(horizontalSpacing)(verticalSpacing)(items)(locked) \
                                (backgroundImageFilename)(backgroundWidth)(backgroundHeight)(backgroundOpacity)

}

#endif  //EC2_LAYOUT_DATA_H
