
#ifndef CAMERA_MANAGER_H
#define CAMERA_MANAGER_H

#include <core/resource/camera_resource.h>
#include "nx_ec/ec_api.h"
#include "nx_ec/data/camera_data.h"
#include "transaction/transaction.h"
#include "nx_ec/data/camera_server_item_data.h"


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
        virtual int getCameras( QnId mediaServerId, impl::GetCamerasHandlerPtr handler ) override;
        //!Implementation of AbstractCameraManager::getCameraHistoryList
        virtual int getCameraHistoryList( impl::GetCamerasHistoryHandlerPtr handler ) override;
        //!Implementation of AbstractCameraManager::save
        virtual int save( const QnVirtualCameraResourceList& cameras, impl::AddCameraHandlerPtr handler ) override;
        //!Implementation of AbstractCameraManager::remove
        virtual int remove( const QnId& id, impl::SimpleHandlerPtr handler ) override;

        template<class T> void triggerNotification( const QnTransaction<T>& tran ) {
            static_assert( false, "Specify QnCameraManager::triggerNotification<>, please" );
        }

        template<> void triggerNotification<ApiCameraData>( const QnTransaction<ApiCameraData>& tran )
        {
            assert( tran.command == ApiCommand::addCamera || tran.command == ApiCommand::updateCamera );
            QnVirtualCameraResourcePtr cameraRes = m_resCtx.resFactory->createResource(
                tran.params.typeId,
                tran.params.toHashMap() ).dynamicCast<QnVirtualCameraResource>();
            tran.params.toResource( cameraRes );
            emit cameraAddedOrUpdated( cameraRes );
        }

        template<> void triggerNotification<ApiCameraDataList>( const QnTransaction<ApiCameraDataList>& tran )
        {
            assert( tran.command == ApiCommand::updateCameras );
            foreach(const ApiCameraData& camera, tran.params.data) 
            {
                QnVirtualCameraResourcePtr cameraRes = m_resCtx.resFactory->createResource(
                    camera.typeId,
                    camera.toHashMap() ).dynamicCast<QnVirtualCameraResource>();
                camera.toResource( cameraRes );
                emit cameraAddedOrUpdated( cameraRes );
            }
        }

        template<> void triggerNotification<ApiIdData>( const QnTransaction<ApiIdData>& tran )
        {
            assert( tran.command == ApiCommand::removeCamera );
            emit cameraRemoved( QnId(tran.params.id) );
        }

        template<> void triggerNotification<ApiCameraServerItemData>( const QnTransaction<ApiCameraServerItemData>& tran )
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
