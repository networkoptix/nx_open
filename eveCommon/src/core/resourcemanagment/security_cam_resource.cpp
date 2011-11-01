#include "security_cam_resource.h"

QnSequrityCamResource::QnSequrityCamResource()
{
    addFlag(QnResource::live_cam);
}

QnSequrityCamResource::~QnSequrityCamResource()
{

}

QString QnSequrityCamResource::oemName() const
{
    return manufacture();
}


int QnSequrityCamResource::getMaxFps()
{
    if (hasSuchParam("MaxFPS"))
    {
        Q_ASSERT(false);
        return 15;
    }

    QnValue val;
    getParam("MaxFPS", val, QnDomainMemory);
    return val;
}

QSize QnSequrityCamResource::getMaxSensorSize()
{

    if (!hasSuchParam("MaxSensorWidth") || !hasSuchParam("MaxSensorHeight"))
    {
        Q_ASSERT(false);
        return QSize(0,0);
    }

    QnValue val_w, val_h;
    getParam("MaxSensorWidth", val_w, QnDomainMemory);
    getParam("MaxSensorHeight", val_h, QnDomainMemory);

    return QSize(val_w, val_h);

}

QRect QnSequrityCamResource::getCroping(QnDomain domain)
{
    return QRect(0, 0, 100, 100);
}

void QnSequrityCamResource::setCroping(QRect croping, QnDomain domain)
{
    setCropingPhysical(croping);
}

