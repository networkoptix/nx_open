#ifndef __DW_DVR_RESOURCE_H__
#define __DW_DVR_RESOURCE_H__

#include "dw_dvr_resource.h"

const char* QnDwDvrResource::MANUFACTURE = "Digital watchdag";

QString QnDwDvrResource::manufacture() const
{
    return MANUFACTURE;
}

void QnDwDvrResource::setMotionMaskPhysical(int channel)
{

}

#endif // __DW_DVR_RESOURCE_H__
