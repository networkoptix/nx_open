#include "camera_data_handler.h"
#include "core/resource_management/resource_pool.h"
#include "core/resource/network_resource.h"
#include "common/common_module.h"

namespace ec2
{

QByteArray QnMutexCameraDataHandler::getUserData(const QString& name)
{
#ifdef EDGE_SERVER
    char  mac[MAC_ADDR_LEN];
    char* host = 0;
    getMacFromPrimaryIF(mac, &host);
    if (name.toLocal8Bit() == QByteArray(mac))
        return qnCommon->moduleGUID().toRfc4122(); // block edge camera discovered from other PC
#endif

    QnNetworkResourcePtr camRes = qnResPool->getNetResourceByPhysicalId(name);
    if (camRes)
        return qnCommon->moduleGUID().toRfc4122();
    else
        return QByteArray();
}

bool QnMutexCameraDataHandler::checkUserData(const QString& /*name*/, const QByteArray& data)
{
    return data.isEmpty() || data == qnCommon->moduleGUID().toRfc4122();
}

}
