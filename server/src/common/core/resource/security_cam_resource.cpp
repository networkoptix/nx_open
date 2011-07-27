#include "security_cam_resource.h"


QnSequrityCamResource::QnSequrityCamResource()
{
    addFlag(QnResource::live_cam);
}

void QnSequrityCamResource::getMaxFps() const
{
    return 30;
}

QRect QnSequrityCamResource::getCroping(QnDomain domain)
{
    return QRect();
}

void QnSequrityCamResource::setCroping(QRect croping, QnDomain domain)
{
    //setCropingPhysical(croping);
}
