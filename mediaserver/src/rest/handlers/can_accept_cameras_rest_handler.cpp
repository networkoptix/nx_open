#include <QFuture>
#include <QtConcurrent>

#include "can_accept_cameras_rest_handler.h"

#include <api/model/camera_list_reply.h>
#include <common/common_module.h>
#include <media_server/serverutil.h>
#include <utils/network/http/httptypes.h>
#include "core/resource_management/resource_discovery_manager.h"
#include "core/resource/camera_resource.h"
#include "core/resource_management/resource_pool.h"


static QnResourceList CheckHostAddrAsync(const QnManualCameraInfo& input) { 
    return input.checkHostAddr(); 
}

int QnCanAcceptCameraRestHandler::executePost(const QString &path, const QnRequestParams &params, const QByteArray &body, QnJsonRestResult &result, const QnRestConnectionProcessor*)
{
    Q_UNUSED(path)
    Q_UNUSED(params)

    QnCameraListReply inCameras;
    QnCameraListReply outCameras;
    QnCameraListReply discoveredCameras;
    //QnSecurityCamResourceList manualCamList;
    QnManualCameraInfoMap manualCamList;
    QJson::deserialize(body, &inCameras);

    for(const QString& uniqueeId: QnResourceDiscoveryManager::instance()->lastDiscoveredIds())
        discoveredCameras.uniqueIdList << uniqueeId;

    for(const QString& uniqueID: inCameras.uniqueIdList) {
        if (discoveredCameras.uniqueIdList.contains(uniqueID)) {
            outCameras.uniqueIdList << uniqueID;
        }
        else {
            // check for manual camera
            QnSecurityCamResourcePtr camera = qnResPool->getResourceByUniqId(uniqueID).dynamicCast<QnSecurityCamResource>();
            if (camera && camera->isManuallyAdded())
                QnResourceDiscoveryManager::instance()->fillManualCamInfo(manualCamList, camera);
        }
    }
    // add manual cameras
    QFuture<QnResourceList> results = QtConcurrent::mapped(manualCamList, &CheckHostAddrAsync);
    results.waitForFinished();
    for (QFuture<QnResourceList>::const_iterator itr = results.constBegin(); itr != results.constEnd(); ++itr)
    {
        QnResourceList foundResources = *itr;
        for (int i = 0; i < foundResources.size(); ++i) {
            QnSecurityCamResourcePtr camera = qSharedPointerDynamicCast<QnSecurityCamResource>(foundResources.at(i));
            if (camera)
                outCameras.uniqueIdList << camera->getUniqueId();
        }
    }


    result.setReply( outCameras );
    return nx_http::StatusCode::ok;
}
