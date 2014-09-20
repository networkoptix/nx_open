#ifndef CAMERA_MANAGER_H
#define CAMERA_MANAGER_H

#include <core/resource/camera_resource.h>
#include <core/resource/resource_factory.h>

#include "nx_ec/ec_api.h"
#include "nx_ec/data/api_camera_data.h"
#include "transaction/transaction.h"
#include "nx_ec/data/api_camera_server_item_data.h"
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
            emit cameraRemoved( QUuid(tran.params.id) );
        }

        void triggerNotification( const QnTransaction<ApiCameraServerItemData>& tran )
        {
            QnCameraHistoryItemPtr cameraHistoryItem( new QnCameraHistoryItem(
                tran.params.cameraUniqueId,
                tran.params.timestamp,
                tran.params.serverId ) );
            emit cameraHistoryChanged( cameraHistoryItem );
        }

        void triggerNotification( const QnTransaction<ApiCameraBookmarkTagDataList>& tran )
        {
            QnCameraBookmarkTags tags;
            fromApiToResource(tran.params, tags);
            if (tags.isEmpty())
                return;

            switch (tran.command) {
            case ApiCommand::addCameraBookmarkTags:
                emit cameraBookmarkTagsAdded(tags);
                break;
            case ApiCommand::removeCameraBookmarkTags:
                emit cameraBookmarkTagsRemoved(tags);
                break;
            default:
                assert(false);  //should never get here
                break;
            }
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
        //!Implementation of AbstractCameraManager::addCameraHistoryItem
        virtual int addCameraHistoryItem( const QnCameraHistoryItem& cameraHistoryItem, impl::SimpleHandlerPtr handler ) override;
        //!Implementation of AbstractCameraManager::removeCameraHistoryItem
        virtual int removeCameraHistoryItem( const QnCameraHistoryItem& cameraHistoryItem, impl::SimpleHandlerPtr handler ) override;
        //!Implementation of AbstractCameraManager::getCameras
        virtual int getCameras( const QUuid& mediaServerId, impl::GetCamerasHandlerPtr handler ) override;
        //!Implementation of AbstractCameraManager::getCameraHistoryList
        virtual int getCameraHistoryList( impl::GetCamerasHistoryHandlerPtr handler ) override;
        //!Implementation of AbstractCameraManager::save
        virtual int save( const QnVirtualCameraResourceList& cameras, impl::AddCameraHandlerPtr handler ) override;
        //!Implementation of AbstractCameraManager::remove
        virtual int remove( const QUuid& id, impl::SimpleHandlerPtr handler ) override;

        //!Implementation of AbstractCameraManager::addBookmarkTags
        virtual int addBookmarkTags(const QnCameraBookmarkTags &tags, impl::SimpleHandlerPtr handler) override;
        //!Implementation of AbstractCameraManager::getBookmarkTags
        virtual int getBookmarkTags(impl::GetCameraBookmarkTagsHandlerPtr handler) override;
        //!Implementation of AbstractCameraManager::removeBookmarkTags
        virtual int removeBookmarkTags(const QnCameraBookmarkTags &tags, impl::SimpleHandlerPtr handler) override;

    private:
        QueryProcessorType* const m_queryProcessor;

        QnTransaction<ApiCameraData> prepareTransaction( ApiCommand::Value cmd, const QnVirtualCameraResourcePtr& resource );
        QnTransaction<ApiCameraDataList> prepareTransaction( ApiCommand::Value cmd, const QnVirtualCameraResourceList& cameras );
        QnTransaction<ApiCameraServerItemData> prepareTransaction( ApiCommand::Value cmd, const QnCameraHistoryItem& historyItem );
        QnTransaction<ApiIdData> prepareTransaction( ApiCommand::Value command, const QUuid& id );
        QnTransaction<ApiCameraBookmarkTagDataList> prepareTransaction( ApiCommand::Value command, const QnCameraBookmarkTags& tags );
    };
}

#endif  //CAMERA_MANAGER_H
