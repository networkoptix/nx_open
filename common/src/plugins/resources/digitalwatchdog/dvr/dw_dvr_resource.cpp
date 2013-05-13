#ifndef __DW_DVR_RESOURCE_H__
#define __DW_DVR_RESOURCE_H__

#include "dw_dvr_resource.h"

const char* QnDwDvrResource::MANUFACTURE = "Digital watchdog";

QString QnDwDvrResource::manufacture() const
{
    return QLatin1String(MANUFACTURE);
}

void QnDwDvrResource::setMotionMaskPhysical(int channel)
{
    Q_UNUSED(channel)
}

#endif // __DW_DVR_RESOURCE_H__
