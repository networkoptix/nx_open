
#include "workbench_item_helper.h"

#include <core/resource/camera_resource.h>

QnVirtualCameraResourcePtr helpers::extractCameraResource(QnWorkbenchItem *item)
{
    return extractResource<QnVirtualCameraResource>(item);
}
