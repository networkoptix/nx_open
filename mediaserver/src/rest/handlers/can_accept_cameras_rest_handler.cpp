#include <QFuture>
#include <QtConcurrent>

#include "can_accept_cameras_rest_handler.h"

#include <api/model/camera_list_reply.h>
#include <common/common_module.h>
#include <media_server/serverutil.h>
#include <utils/common/concurrent.h>
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
    //QnSecurityCamResourceList manualCamList;
    QnManualCameraInfoMap manualCamList;
    std::deque<QnSecurityCamResourcePtr> camerasToPing;
    QJson::deserialize(body, &inCameras);

    const QSet<QString>& discoveredCameras = QnResourceDiscoveryManager::instance()->lastDiscoveredIds();

    for(const QString& uniqueID: inCameras.uniqueIdList)
    {
        if (discoveredCameras.contains(uniqueID))
        {
            outCameras.uniqueIdList << uniqueID;
            continue;
        }

        // check for manual camera
        QnSecurityCamResourcePtr camera = qnResPool->getResourceByUniqId(uniqueID).dynamicCast<QnSecurityCamResource>();
        if( !camera )
            continue;

        if (camera->isManuallyAdded())
        {
            QnResourceDiscoveryManager::instance()->fillManualCamInfo(manualCamList, camera);
            continue;
        }
        
        //camera resource exists and camera was discovered by auto discovery,
            //but camera was not found by recent auto discovery, trying to discover camera using unicast
            //E.g., some Arecont cameras do not answer discovery requests while they are being recorded
        camerasToPing.push_back( std::move(camera) );
    }

    // add manual cameras
    QFuture<QnResourceList> manualDiscoveryResults = QtConcurrent::mapped(manualCamList, &CheckHostAddrAsync);
    //checking cameras with unicast
    QnConcurrent::QnFuture<bool> camerasToPingResults = QnConcurrent::mapped(
        camerasToPing,
        [](const QnSecurityCamResourcePtr& camera) -> bool { return camera->ping(); } );

    manualDiscoveryResults.waitForFinished();
    for (QFuture<QnResourceList>::const_iterator
        itr = manualDiscoveryResults.constBegin();
        itr != manualDiscoveryResults.constEnd();
        ++itr)
    {
        const QnResourceList& foundResources = *itr;
        for (int i = 0; i < foundResources.size(); ++i) {
            const QnSecurityCamResource* camera = dynamic_cast<const QnSecurityCamResource*>(foundResources.at(i).data());
            if (camera)
                outCameras.uniqueIdList << camera->getUniqueId();
        }
    }

    camerasToPingResults.waitForFinished();
    for( size_t i = 0; i < camerasToPing.size(); ++i )
    {
        if( !camerasToPingResults.resultAt(i) )
            continue;
        outCameras.uniqueIdList << camerasToPing[i]->getUniqueId();
    }

    result.setReply( outCameras );
    return nx_http::StatusCode::ok;
}
