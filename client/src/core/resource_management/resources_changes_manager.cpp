#include "resources_changes_manager.h"

#include <api/app_server_connection.h>

#include <common/common_module.h>

#include <core/resource_management/resource_pool.h>
#include <core/resource_management/resource_properties.h>

#include <core/resource/camera_resource.h>
#include <core/resource/camera_user_attribute_pool.h>

#include <core/resource/media_server_resource.h>
#include <core/resource/media_server_user_attributes.h>



QnResourcesChangesManager::QnResourcesChangesManager(QObject* parent /*= nullptr*/):
    base_type(parent)
{}

QnResourcesChangesManager::~QnResourcesChangesManager()
{}

/************************************************************************/
/* Cameras block                                                        */
/************************************************************************/

void QnResourcesChangesManager::saveCamera(const QnVirtualCameraResourcePtr &camera, CameraChangesFunction applyChanges) {
    saveCameras(QnVirtualCameraResourceList() << camera, applyChanges);
}

void QnResourcesChangesManager::saveCameras(const QnVirtualCameraResourceList &cameras, CameraChangesFunction applyChanges) {
    if (!applyChanges)
        return;

     auto batchFunction = [cameras, applyChanges]() {
         for (const QnVirtualCameraResourcePtr &camera: cameras)
             applyChanges(camera);
     };
     saveCamerasBatch(cameras, batchFunction);
}

void QnResourcesChangesManager::saveCamerasBatch(const QnVirtualCameraResourceList &cameras, 
                                                 BatchChangesFunction applyChanges, 
                                                 RollbackFunction rollback) {
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

    applyChanges(); 
    auto changes = pool->getAttributesList(idList);   

    //TODO: #GDM SafeMode
    QnAppServerConnectionFactory::getConnection2()->getCameraManager()->saveUserAttributes(changes, this, 
        [cameras, pool, backup, sessionGuid, rollback]( int reqID, ec2::ErrorCode errorCode ) 
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

        if (rollback)
            rollback();
    });

    //TODO: #GDM SafeMode values are not rolled back
    propertyDictionary->saveParamsAsync(idList);  
}

/************************************************************************/
/* Servers block                                                        */
/************************************************************************/

void QnResourcesChangesManager::saveServer(const QnMediaServerResourcePtr &server, ServerChangesFunction applyChanges) {
    saveServers(QnMediaServerResourceList() << server, applyChanges);
}

void QnResourcesChangesManager::saveServers(const QnMediaServerResourceList &servers, ServerChangesFunction applyChanges) {
    if (!applyChanges)
        return;

    auto batchFunction = [servers, applyChanges]() {
        for (const QnMediaServerResourcePtr &server: servers)
            applyChanges(server);
    };
    saveServersBatch(servers, batchFunction);
}

void QnResourcesChangesManager::saveServersBatch(const QnMediaServerResourceList &servers,
                                                 BatchChangesFunction applyChanges, 
                                                 RollbackFunction rollback) {
    if (!applyChanges)
        return;

    auto idList = idListFromResList(servers);

    QPointer<QnMediaServerUserAttributesPool> pool(QnMediaServerUserAttributesPool::instance());   

    QList<QnMediaServerUserAttributes> backup;
    for(const QnMediaServerUserAttributesPtr& serverAttrs: pool->getAttributesList(idList)) {
        QnMediaServerUserAttributesPool::ScopedLock userAttributesLock( pool, serverAttrs->serverID );
        backup << *(*userAttributesLock);
    }

    auto sessionGuid = qnCommon->runningInstanceGUID();

    applyChanges(); 
    auto changes = pool->getAttributesList(idList);   

    //TODO: #GDM SafeMode
    QnAppServerConnectionFactory::getConnection2()->getMediaServerManager()->saveUserAttributes(changes, this, 
        [servers, pool, backup, sessionGuid, rollback]( int reqID, ec2::ErrorCode errorCode ) 
    {
        Q_UNUSED(reqID);

        /* Check if all OK */
        if (errorCode == ec2::ErrorCode::ok)
            return;

        /* Check if we have already changed session or attributes pool was recreated. */
        if (!pool || qnCommon->runningInstanceGUID() != sessionGuid)
            return;

        /* Restore attributes from backup. */
        for( const QnMediaServerUserAttributes& serverAttrs: backup ) {
            QSet<QByteArray> modifiedFields;
            {
                QnMediaServerUserAttributesPool::ScopedLock userAttributesLock( pool, serverAttrs.serverID );
                (*userAttributesLock)->assign( serverAttrs, &modifiedFields );
            }            
            if( const QnResourcePtr& res = qnResPool->getResourceById(serverAttrs.serverID) )   //it is OK if resource is missing
                res->emitModificationSignals( modifiedFields );
        }

        if (rollback)
            rollback();
    });

    //TODO: #GDM SafeMode values are not rolled back
    propertyDictionary->saveParamsAsync(idList);  
}
