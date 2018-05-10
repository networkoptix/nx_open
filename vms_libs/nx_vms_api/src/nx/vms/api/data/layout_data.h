#pragma once

#include "resource_data.h"

#include <nx/utils/latin1_array.h>

namespace nx {
namespace vms {
namespace api {

struct LayoutItemData: IdData
{
    qint32 flags = 0;
    float left = 0;
    float top = 0;
    float right = 0;
    float bottom = 0;
    float rotation = 0;
    QnUuid resourceId;
    QString resourcePath;
    float zoomLeft = 0;
    float zoomTop = 0;
    float zoomRight = 0;
    float zoomBottom = 0;
    QnUuid zoomTargetId;
    QnLatin1Array contrastParams; //< TODO: #API Expand this one.
    QnLatin1Array dewarpingParams; //< TODO: #API Expand this one.
    bool displayInfo = false; /**< Should info be displayed on the item. */
};
#define LayoutItemData_Fields IdData_Fields (flags)(left)(top)(right)(bottom)(rotation) \
    (resourceId)(resourcePath)(zoomLeft)(zoomTop)(zoomRight)(zoomBottom)(zoomTargetId) \
    (contrastParams)(dewarpingParams)(displayInfo)

struct LayoutData: ResourceData
{
    float cellAspectRatio = 0;
    float horizontalSpacing = 0;
    float verticalSpacing = 0;
    LayoutItemDataList items;
    bool locked = false;
    QString backgroundImageFilename;
    qint32 backgroundWidth = 0;
    qint32 backgroundHeight = 0;
    float backgroundOpacity = 0;
};
#define LayoutData_Fields ResourceData_Fields (cellAspectRatio)(horizontalSpacing) \
    (verticalSpacing)(items)(locked)(backgroundImageFilename)(backgroundWidth) \
    (backgroundHeight)(backgroundOpacity)

} // namespace api
} // namespace vms
} // namespace nx

Q_DECLARE_METATYPE(nx::vms::api::LayoutItemData)
Q_DECLARE_METATYPE(nx::vms::api::LayoutData)
