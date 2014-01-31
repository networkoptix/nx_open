
#ifndef CAMERA_MANAGER_H
#define CAMERA_MANAGER_H

#include "nx_ec/ec_api.h"
#include "nx_ec/data/camera_data.h"
#include "transaction/transaction.h"


namespace ec2
{
    template<class QueryProcessorType>
    class QnCameraManager
    :
        public AbstractCameraManager
    {
    public:
        QnCameraManager( QueryProcessorType* const queryProcessor, QSharedPointer<QnResourceFactory> factory);

        //!Implementation of AbstractCameraManager::addCamera
        virtual ReqID addCamera( const QnVirtualCameraResourcePtr&, impl::AddCameraHandlerPtr handler ) override;
        //!Implementation of AbstractCameraManager::addCameraHistoryItem
        virtual ReqID addCameraHistoryItem( const QnCameraHistoryItem& cameraHistoryItem, impl::SimpleHandlerPtr handler ) override;
        //!Implementation of AbstractCameraManager::getCameras
        virtual ReqID getCameras( QnId mediaServerId, impl::GetCamerasHandlerPtr handler ) override;
        //!Implementation of AbstractCameraManager::getCameraHistoryList
        virtual ReqID getCameraHistoryList( impl::GetCamerasHistoryHandlerPtr handler ) override;
        //!Implementation of AbstractCameraManager::save
        virtual ReqID save( const QnVirtualCameraResourceList& cameras, impl::SimpleHandlerPtr handler ) override;
        //!Implementation of AbstractCameraManager::remove
        virtual ReqID remove( const QnVirtualCameraResourcePtr& resource, impl::SimpleHandlerPtr handler ) override;

    private:
        QueryProcessorType* const m_queryProcessor;
		QSharedPointer<QnResourceFactory> m_resourceFactory;

        QnTransaction<ApiCameraData> prepareTransaction( ApiCommand::Value cmd, const QnVirtualCameraResourcePtr& resource );
    };
}

#endif  //CAMERA_MANAGER_H
