#ifndef __DW_DVR_RESOURCE_H__
#define __DW_DVR_RESOURCE_H__

#include "dw_dvr_resource.h"

const QString QnDwDvrResource::MANUFACTURE(lit("Digital watchdog"));

QString QnDwDvrResource::getDriverName() const
{
    return MANUFACTURE;
}

void QnDwDvrResource::setMotionMaskPhysical(int channel)
{
    Q_UNUSED(channel)
}

#endif // __DW_DVR_RESOURCE_H__
