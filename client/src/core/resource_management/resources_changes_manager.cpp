#include "resources_changes_manager.h"

#include <api/app_server_connection.h>

#include <common/common_module.h>

#include <core/resource_management/resource_pool.h>

#include <core/resource/camera_resource.h>
#include <core/resource/camera_user_attribute_pool.h>


QnResourcesChangesManager::QnResourcesChangesManager(QObject* parent /*= nullptr*/):
    base_type(parent)
{

}

QnResourcesChangesManager::~QnResourcesChangesManager()
{

}

void QnResourcesChangesManager::saveCameras(const QnVirtualCameraResourceList &cameras, CameraChangesFunction applyChanges) {
    if (!applyChanges)
        return;

    auto idList = idListFromResList(cameras);

    QPointer<QnCameraUserAttributePool> pool(QnCameraUserAttributePool::instance());   

    QList<QnCameraUserAttributes> backup;
    for(const QnCameraUserAttributesPtr& cameraAttrs: pool->getAttributesList(idList)) {
        QnCameraUserAttributePool::ScopedLock userAttributesLock( pool, cameraAttrs->cameraID );
        backup << *(*userAttributesLock);
    }

    auto sessionGuid = qnCommon->runningInstanceGUID();

    for (const QnVirtualCameraResourcePtr &camera: cameras)
        applyChanges(camera);
    
    auto changes = pool->getAttributesList(idList);   

    //TODO: #GDM SafeMode
    QnAppServerConnectionFactory::getConnection2()->getCameraManager()->saveUserAttributes(changes, this, 
        [cameras, pool, backup, sessionGuid]( int reqID, ec2::ErrorCode errorCode ) 
    {
        Q_UNUSED(reqID);

        /* Check if all OK */
        if (errorCode == ec2::ErrorCode::ok)
            return;

        /* Check if we have already changed session or attributes pool was recreated. */
        if (!pool || qnCommon->runningInstanceGUID() != sessionGuid)
            return;

        /* Restore attributes from backup. */
        for( const QnCameraUserAttributes& cameraAttrs: backup ) {
            QSet<QByteArray> modifiedFields;
            {
                QnCameraUserAttributePool::ScopedLock userAttributesLock( pool, cameraAttrs.cameraID );
                (*userAttributesLock)->assign( cameraAttrs, &modifiedFields );
            }            
            if( const QnResourcePtr& res = qnResPool->getResourceById(cameraAttrs.cameraID) )   //it is OK if resource is missing
                res->emitModificationSignals( modifiedFields );
        }
    });

}
