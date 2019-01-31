#include "camera_data_handler.h"

#include <nx/network/nettools.h> /* For MAC_ADDR_LEN and getMacFromPrimaryIF. */

#include "core/resource_management/resource_pool.h"
#include "core/resource/camera_resource.h"
#include "core/resource/media_server_resource.h"

#include "common/common_module.h"

namespace ec2
{

const QString QnMutexCameraDataHandler::CAM_INS_PREFIX(lit("CAMERA_INS_"));
const QString QnMutexCameraDataHandler::CAM_UPD_PREFIX(lit("CAMERA_UPD_"));
const QString QnMutexCameraDataHandler::CAM_HISTORY_PREFIX(lit("CAMERA_HISTORY_"));

QnMutexCameraDataHandler::QnMutexCameraDataHandler(QnCommonModule* commonModule):
    QnMutexUserDataHandler(commonModule)
{
}

QByteArray QnMutexCameraDataHandler::getUserData(const QString& name)
{
#ifdef EDGE_SERVER
    char  mac[nx::network::MAC_ADDR_LEN];
    char* host = 0;
    nx::network::getMacFromPrimaryIF(mac, &host);

    if (name.startsWith(CAM_INS_PREFIX) || name.startsWith(CAM_UPD_PREFIX))
    {
        QnMediaServerResourcePtr ownServer = resourcePool()->getResourceById<QnMediaServerResource>(commonModule()->moduleGUID());
        if (ownServer && !ownServer->isRedundancy()) {
            QString cameraName = name.mid(CAM_INS_PREFIX.length());
            if (cameraName.toLocal8Bit() == QByteArray(mac))
                return commonModule()->moduleGUID().toRfc4122(); // block edge camera discovered from other PC
            else
                return QByteArray();
        }
    }
#endif

    const auto& resPool = commonModule()->resourcePool();

    if (name.startsWith(CAM_INS_PREFIX))
    {
        QnNetworkResourcePtr camRes = resPool->getNetResourceByPhysicalId(name.mid(CAM_INS_PREFIX.length()));
        if (camRes)
            return commonModule()->moduleGUID().toRfc4122(); // block
    }
    else if (name.startsWith(CAM_UPD_PREFIX))
    {
        QnSecurityCamResourcePtr camRes = resPool->getNetResourceByPhysicalId(name.mid(CAM_UPD_PREFIX.length())).dynamicCast<QnSecurityCamResource>();
        if (camRes) {
            /* Temporary solution to correctly allow other server to take ownership of desktop camera. */
            if (camRes->hasFlags(Qn::desktop_camera) && camRes->isReadyToDetach())
                return QByteArray(); // do not block desktop cameras that are ready to detach

            if (camRes->preferredServerId() == commonModule()->moduleGUID())
                return commonModule()->moduleGUID().toRfc4122(); // block
            QnResourcePtr mServer = resPool->getResourceById(camRes->getParentId());
            if (mServer && mServer->getStatus() == Qn::Online)
                return mServer->getId().toRfc4122(); // block
        }
    }
    else if (name.startsWith(CAM_HISTORY_PREFIX))
    {
        QnSecurityCamResourcePtr camRes = resPool->getNetResourceByPhysicalId(name.mid(CAM_HISTORY_PREFIX.length())).dynamicCast<QnSecurityCamResource>();
        if (camRes) {
            if (camRes->getParentId() == commonModule()->moduleGUID())
                return commonModule()->moduleGUID().toRfc4122(); // block
        }
    }

    return QByteArray();
}

bool QnMutexCameraDataHandler::checkUserData(const QString& name, const QByteArray& data)
{
    if (name.startsWith(CAM_INS_PREFIX) || name.startsWith(CAM_UPD_PREFIX) || name.startsWith(CAM_HISTORY_PREFIX)) {
        return data.isEmpty() || data == commonModule()->moduleGUID().toRfc4122();
    }
    return true;
}

}
