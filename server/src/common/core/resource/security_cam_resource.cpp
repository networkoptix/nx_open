#include "security_cam_resource.h"


QnSequrityCamResource::QnSequrityCamResource()
{
    addFlag(QnResource::live_cam);
}

int QnSequrityCamResource::getMaxFps()
{
    if (hasSuchParam("MaxFPS"))
    {
        Q_ASSERT(false);
        return 30;
    }

    QnValue val;
    getParam("MaxFPS", val, QnDomainMemory);
    return val;
}

QSize QnSequrityCamResource::getMaxSensorSize() const
{

}

QRect QnSequrityCamResource::getCroping(QnDomain domain)
{
    return QRect(0, 0, 100, 100);
}

void QnSequrityCamResource::setCroping(QRect croping, QnDomain domain)
{
    //setCropingPhysical(croping);
}
