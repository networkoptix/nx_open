#ifndef CAMERA_MANAGER_H
#define CAMERA_MANAGER_H

#include <core/resource/camera_resource.h>
#include <core/resource/resource_factory.h>

#include "nx_ec/ec_api.h"
#include "nx_ec/data/api_camera_data.h"
#include "nx_ec/data/api_camera_attributes_data.h"
#include "transaction/transaction.h"
#include "nx_ec/data/api_camera_history_data.h"
#include "nx_ec/data/api_conversion_functions.h"


namespace ec2
{
    class QnCameraNotificationManager
    :
        public AbstractCameraManager
    {
    public:
        QnCameraNotificationManager( const ResourceContext& resCtx ) : m_resCtx( resCtx ) {}

        void triggerNotification( const QnTransaction<ApiCameraData>& tran )
        {
            NX_ASSERT( tran.command == ApiCommand::saveCamera);
            QnVirtualCameraResourcePtr cameraRes = m_resCtx.resFactory->createResource(
                tran.params.typeId,
                QnResourceParams(tran.params.id, tran.params.url, tran.params.vendor) ).dynamicCast<QnVirtualCameraResource>();
            NX_ASSERT(cameraRes, Q_FUNC_INFO, QByteArray("Unknown resource type:") + tran.params.typeId.toByteArray());
            if (cameraRes) {
                fromApiToResource(tran.params, cameraRes);
                emit cameraAddedOrUpdated( std::move(cameraRes ));
            }
        }

        void triggerNotification( const QnTransaction<ApiCameraDataList>& tran )
        {
            NX_ASSERT( tran.command == ApiCommand::saveCameras );
            for(const ApiCameraData& camera: tran.params) 
            {
                QnVirtualCameraResourcePtr cameraRes = m_resCtx.resFactory->createResource(
                    camera.typeId,
                    QnResourceParams(camera.id, camera.url, camera.vendor) ).dynamicCast<QnVirtualCameraResource>();
                NX_ASSERT( cameraRes );
                fromApiToResource(camera, cameraRes);
                emit cameraAddedOrUpdated( std::move(cameraRes) );
            }
        }

        void triggerNotification( const QnTransaction<ApiCameraAttributesData>& tran ) {
            NX_ASSERT( tran.command == ApiCommand::saveCameraUserAttributes );
            QnCameraUserAttributesPtr cameraAttrs( new QnCameraUserAttributes() );
            fromApiToResource( tran.params, cameraAttrs );
            emit userAttributesChanged( std::move(cameraAttrs) );
        }

        void triggerNotification( const QnTransaction<ApiCameraAttributesDataList>& tran ) {
            NX_ASSERT( tran.command == ApiCommand::saveCameraUserAttributesList );
            for(const ApiCameraAttributesData& attrs: tran.params) 
            {
                QnCameraUserAttributesPtr cameraAttrs( new QnCameraUserAttributes() );
                fromApiToResource( attrs, cameraAttrs );
                emit userAttributesChanged( std::move(cameraAttrs) );
            }
        }

        void triggerNotification( const QnTransaction<ApiIdData>& tran )
        {
            NX_ASSERT( tran.command == ApiCommand::removeCamera );
            emit cameraRemoved( QnUuid(tran.params.id) );
        }

        void triggerNotification( const QnTransaction<ApiServerFootageData>& tran )
        {
            if (tran.command == ApiCommand::addCameraHistoryItem)
                emit cameraHistoryChanged( tran.params);
            /*
            QnCameraHistoryItemPtr cameraHistoryItem( new QnCameraHistoryItem(
                tran.params.cameraUniqueId,
                tran.params.timestamp,
                tran.params.serverGuid ) );
            if (tran.command == ApiCommand::addCameraHistoryItem)
                emit cameraHistoryChanged( std::move(cameraHistoryItem ));
            else
                emit cameraHistoryRemoved( std::move(cameraHistoryItem ));
            */
        }

    protected:
		ResourceContext m_resCtx;
    };




    template<class QueryProcessorType>
    class QnCameraManager
    :
        public QnCameraNotificationManager
    {
    public:
        QnCameraManager( QueryProcessorType* const queryProcessor, const ResourceContext& resCtx );

        //!Implementation of AbstractCameraManager::addCamera
        virtual int addCamera( const QnVirtualCameraResourcePtr&, impl::AddCameraHandlerPtr handler ) override;
        //!Implementation of AbstractCameraManager::ApiServerFootageData
        virtual int setCamerasWithArchive(const QnUuid& serverGuid, const std::vector<QnUuid>& cameras, impl::SimpleHandlerPtr handler ) override;
        //!Implementation of AbstractCameraManager::getCameras
        virtual int getCameras( const QnUuid& mediaServerId, impl::GetCamerasHandlerPtr handler ) override;
        //!Implementation of AbstractCameraManager::getCameraHistoryList
        virtual int getCamerasWithArchiveList( impl::GetCamerasHistoryHandlerPtr handler ) override;
        //!Implementation of AbstractCameraManager::save
        virtual int save( const QnVirtualCameraResourceList& cameras, impl::AddCameraHandlerPtr handler ) override;
        //!Implementation of AbstractCameraManager::saveUserAttributes
        virtual int saveUserAttributes( const QnCameraUserAttributesList& cameraAttributes, impl::SimpleHandlerPtr handler ) override;
        //!Implementation of AbstractCameraManager::getUserAttributes
        virtual int getUserAttributes( const QnUuid& serverId, impl::GetCameraUserAttributesHandlerPtr handler ) override;
        //!Implementation of AbstractCameraManager::remove
        virtual int remove( const QnUuid& id, impl::SimpleHandlerPtr handler ) override;

    private:
        QueryProcessorType* const m_queryProcessor;

        QnTransaction<ApiCameraData> prepareTransaction( ApiCommand::Value cmd, const QnVirtualCameraResourcePtr& resource );
        QnTransaction<ApiCameraDataList> prepareTransaction( ApiCommand::Value cmd, const QnVirtualCameraResourceList& cameras );
        QnTransaction<ApiCameraAttributesDataList> prepareTransaction( ApiCommand::Value cmd, const QnCameraUserAttributesList& cameraAttributesList );
        QnTransaction<ApiIdData> prepareTransaction( ApiCommand::Value command, const QnUuid& id );
    };
}

#endif  //CAMERA_MANAGER_H
