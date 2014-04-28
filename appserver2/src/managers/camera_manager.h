
#ifndef CAMERA_MANAGER_H
#define CAMERA_MANAGER_H

#include <core/resource/camera_resource.h>
#include "nx_ec/ec_api.h"
#include "nx_ec/data/api_camera_data.h"
#include "transaction/transaction.h"
#include "nx_ec/data/api_camera_server_item_data.h"
#include "nx_ec/data/api_conversion_functions.h"


namespace ec2
{
    template<class QueryProcessorType>
    class QnCameraManager
    :
        public AbstractCameraManager
    {
    public:
        QnCameraManager( QueryProcessorType* const queryProcessor, const ResourceContext& resCtx );

        //!Implementation of AbstractCameraManager::addCamera
        virtual int addCamera( const QnVirtualCameraResourcePtr&, impl::AddCameraHandlerPtr handler ) override;
        //!Implementation of AbstractCameraManager::addCameraHistoryItem
        virtual int addCameraHistoryItem( const QnCameraHistoryItem& cameraHistoryItem, impl::SimpleHandlerPtr handler ) override;
        //!Implementation of AbstractCameraManager::getCameras
        virtual int getCameras( const QnId& mediaServerId, impl::GetCamerasHandlerPtr handler ) override;
        //!Implementation of AbstractCameraManager::getCameraHistoryList
        virtual int getCameraHistoryList( impl::GetCamerasHistoryHandlerPtr handler ) override;
        //!Implementation of AbstractCameraManager::save
        virtual int save( const QnVirtualCameraResourceList& cameras, impl::AddCameraHandlerPtr handler ) override;
        //!Implementation of AbstractCameraManager::remove
        virtual int remove( const QnId& id, impl::SimpleHandlerPtr handler ) override;

        void triggerNotification( const QnTransaction<ApiCameraData>& tran )
        {
            assert( tran.command == ApiCommand::saveCamera);
            QnVirtualCameraResourcePtr cameraRes = m_resCtx.resFactory->createResource(
                tran.params.typeId,
                QnResourceParams(tran.params.url, tran.params.vendor) ).dynamicCast<QnVirtualCameraResource>();
            Q_ASSERT(cameraRes);
            if (cameraRes) {
                fromApiToResource(tran.params, cameraRes);
                emit cameraAddedOrUpdated( cameraRes );
            }
        }

        void triggerNotification( const QnTransaction<ApiCameraDataList>& tran )
        {
            assert( tran.command == ApiCommand::saveCameras );
            foreach(const ApiCameraData& camera, tran.params) 
            {
                QnVirtualCameraResourcePtr cameraRes = m_resCtx.resFactory->createResource(
                    camera.typeId,
                    QnResourceParams(camera.url, camera.vendor) ).dynamicCast<QnVirtualCameraResource>();
                fromApiToResource(camera, cameraRes);
                emit cameraAddedOrUpdated( cameraRes );
            }
        }

        void triggerNotification( const QnTransaction<ApiIdData>& tran )
        {
            assert( tran.command == ApiCommand::removeCamera );
            emit cameraRemoved( QnId(tran.params.id) );
        }

        void triggerNotification( const QnTransaction<ApiCameraServerItemData>& tran )
        {
            QnCameraHistoryItemPtr cameraHistoryItem( new QnCameraHistoryItem(
                tran.params.physicalId,
                tran.params.timestamp,
                tran.params.serverGuid ) );
            emit cameraHistoryChanged( cameraHistoryItem );
        }

    private:
        QueryProcessorType* const m_queryProcessor;
		ResourceContext m_resCtx;

        QnTransaction<ApiCameraData> prepareTransaction( ApiCommand::Value cmd, const QnVirtualCameraResourcePtr& resource );
        QnTransaction<ApiCameraDataList> prepareTransaction( ApiCommand::Value cmd, const QnVirtualCameraResourceList& cameras );
        QnTransaction<ApiCameraServerItemData> prepareTransaction( ApiCommand::Value cmd, const QnCameraHistoryItem& historyItem );
        QnTransaction<ApiIdData> prepareTransaction( ApiCommand::Value command, const QnId& id );
    };
}

#endif  //CAMERA_MANAGER_H
