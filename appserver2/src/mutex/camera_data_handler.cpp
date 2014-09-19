#include "camera_data_handler.h"

#include "utils/network/nettools.h" /* For MAC_ADDR_LEN and getMacFromPrimaryIF. */

#include "core/resource_management/resource_pool.h"
#include "core/resource/network_resource.h"

#include "common/common_module.h"

namespace ec2
{

const QString QnMutexCameraDataHandler::CAM_INS_PREFIX(lit("CAMERA_INS_"));
const QString QnMutexCameraDataHandler::CAM_UPD_PREFIX(lit("CAMERA_UPD_"));


QByteArray QnMutexCameraDataHandler::getUserData(const QString& name)
{
#ifdef EDGE_SERVER
    char  mac[MAC_ADDR_LEN];
    char* host = 0;
    getMacFromPrimaryIF(mac, &host);

    if (name.startsWith("CAM_INS_PREFIX") || name.startsWith("CAM_UPD_PREFIX"))
        return qnCommon->moduleGUID().toRfc4122(); // block edge camera discovered from other PC
#endif

    if (name.startsWith("CAM_INS_PREFIX"))
    {
        QnNetworkResourcePtr camRes = qnResPool->getNetResourceByPhysicalId(name);
        if (camRes)
            return qnCommon->moduleGUID().toRfc4122(); // block
    }
    else if (name.startsWith("CAM_UPD_PREFIX"))
    {
        QnNetworkResourcePtr camRes = qnResPool->getNetResourceByPhysicalId(name);
        if (camRes) {
            QnResourcePtr mServer = qnResPool->getResourceById(camRes->getParentId());
            if (mServer && mServer->getStatus() == Qn::Online)
                return qnCommon->moduleGUID().toRfc4122(); // block
        }
    }
        
    return QByteArray();
}

bool QnMutexCameraDataHandler::checkUserData(const QString& name, const QByteArray& data)
{
    if (name.startsWith("CAM_INS_PREFIX") || name.startsWith("CAM_UPD_PREFIX")) {
        return data.isEmpty() || data == qnCommon->moduleGUID().toRfc4122();
    }
    return true;
}

}
