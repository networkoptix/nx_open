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


QByteArray QnMutexCameraDataHandler::getUserData(const QString& name)
{
#ifdef EDGE_SERVER
    char  mac[MAC_ADDR_LEN];
    char* host = 0;
    getMacFromPrimaryIF(mac, &host);

    if (name.startsWith(CAM_INS_PREFIX) || name.startsWith(CAM_UPD_PREFIX)) 
    {
        QnMediaServerResourcePtr ownServer = qnResPool->getResourceById<QnMediaServerResource>(qnCommon->moduleGUID());
        if (ownServer && !ownServer->isRedundancy()) {
            QString cameraName = name.mid(CAM_INS_PREFIX.length());
            if (cameraName.toLocal8Bit() == QByteArray(mac))
                return qnCommon->moduleGUID().toRfc4122(); // block edge camera discovered from other PC
            else
                return QByteArray();
        }
    }
#endif

    if (name.startsWith(CAM_INS_PREFIX))
    {
        QnNetworkResourcePtr camRes = qnResPool->getNetResourceByPhysicalId(name.mid(CAM_INS_PREFIX.length()));
        if (camRes)
            return qnCommon->moduleGUID().toRfc4122(); // block
    }
    else if (name.startsWith(CAM_UPD_PREFIX))
    {
        QnSecurityCamResourcePtr camRes = qnResPool->getNetResourceByPhysicalId(name.mid(CAM_UPD_PREFIX.length())).dynamicCast<QnSecurityCamResource>();
        if (camRes) {
            /* Temporary solution to correctly allow other server to take ownership of desktop camera. */
            if (camRes->hasFlags(Qn::desktop_camera) && camRes->isReadyToDetach())
                return QByteArray(); // do not block desktop cameras that are ready to detach

            if (camRes->preferedServerId() == qnCommon->moduleGUID())
                return qnCommon->moduleGUID().toRfc4122(); // block
            QnResourcePtr mServer = qnResPool->getResourceById(camRes->getParentId());
            if (mServer && mServer->getStatus() == Qn::Online)
                return qnCommon->moduleGUID().toRfc4122(); // block
        }
    }
    else if (name.startsWith(CAM_HISTORY_PREFIX))
    {
        QnSecurityCamResourcePtr camRes = qnResPool->getNetResourceByPhysicalId(name.mid(CAM_HISTORY_PREFIX.length())).dynamicCast<QnSecurityCamResource>();
        if (camRes) {
            if (camRes->getParentId() == qnCommon->moduleGUID())
                return qnCommon->moduleGUID().toRfc4122(); // block
        }
    }
        
    return QByteArray();
}

bool QnMutexCameraDataHandler::checkUserData(const QString& name, const QByteArray& data)
{
    if (name.startsWith(CAM_INS_PREFIX) || name.startsWith(CAM_UPD_PREFIX) || name.startsWith(CAM_HISTORY_PREFIX)) {
        return data.isEmpty() || data == qnCommon->moduleGUID().toRfc4122();
    }
    return true;
}

}
