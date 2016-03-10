#pragma once

#include <transaction/transaction.h>
#include <nx_ec/managers/abstract_camera_manager.h>

namespace ec2
{
    class QnCameraNotificationManager: public AbstractCameraManager
    {
    public:
        QnCameraNotificationManager() {}

        void triggerNotification( const QnTransaction<ApiCameraData>& tran )
        {
            assert( tran.command == ApiCommand::saveCamera);
            emit addedOrUpdated(tran.params);
        }

        void triggerNotification( const QnTransaction<ApiCameraDataList>& tran )
        {
            assert( tran.command == ApiCommand::saveCameras );
            for(const ApiCameraData& camera: tran.params)
            {
                emit addedOrUpdated(camera);
            }
        }

        void triggerNotification( const QnTransaction<ApiCameraAttributesData>& tran ) {
            assert( tran.command == ApiCommand::saveCameraUserAttributes );
            emit userAttributesChanged(tran.params);
        }

        void triggerNotification( const QnTransaction<ApiCameraAttributesDataList>& tran ) {
            assert( tran.command == ApiCommand::saveCameraUserAttributesList );
            for(const ApiCameraAttributesData& attrs: tran.params)
                emit userAttributesChanged(attrs);
        }

        void triggerNotification( const QnTransaction<ApiIdData>& tran )
        {
            assert( tran.command == ApiCommand::removeCamera );
            emit removed( tran.params.id );
        }

        void triggerNotification( const QnTransaction<ApiServerFootageData>& tran )
        {
            if (tran.command == ApiCommand::addCameraHistoryItem)
                emit cameraHistoryChanged( tran.params);
        }
    };




    template<class QueryProcessorType>
    class QnCameraManager: public QnCameraNotificationManager
    {
    public:
        QnCameraManager( QueryProcessorType* const queryProcessor );

        //!Implementation of AbstractCameraManager::getCameras
        virtual int getCameras(const QnUuid& mediaServerId, impl::GetCamerasHandlerPtr handler) override;
        //!Implementation of AbstractCameraManager::addCamera
        virtual int addCamera(const ec2::ApiCameraData&, impl::SimpleHandlerPtr handler) override;
        //!Implementation of AbstractCameraManager::save
        virtual int save(const ec2::ApiCameraDataList& cameras, impl::SimpleHandlerPtr handler) override;
        //!Implementation of AbstractCameraManager::remove
        virtual int remove(const QnUuid& id, impl::SimpleHandlerPtr handler) override;

        //!Implementation of AbstractCameraManager::ApiServerFootageData
        virtual int setServerFootageData(const QnUuid& serverGuid, const std::vector<QnUuid>& cameras, impl::SimpleHandlerPtr handler ) override;
        //!Implementation of AbstractCameraManager::getCameraHistoryList
        virtual int getServerFootageData( impl::GetCamerasHistoryHandlerPtr handler ) override;
        //!Implementation of AbstractCameraManager::saveUserAttributes
        virtual int saveUserAttributes( const ec2::ApiCameraAttributesDataList& cameraAttributes, impl::SimpleHandlerPtr handler ) override;
        //!Implementation of AbstractCameraManager::getUserAttributes
        virtual int getUserAttributes( const QnUuid& serverId, impl::GetCameraUserAttributesHandlerPtr handler ) override;

    private:
        QueryProcessorType* const m_queryProcessor;

        QnTransaction<ApiCameraData> prepareTransaction( ApiCommand::Value cmd, const ec2::ApiCameraData& camera);
        QnTransaction<ApiCameraDataList> prepareTransaction( ApiCommand::Value cmd, const ec2::ApiCameraDataList& cameras );

        QnTransaction<ApiCameraAttributesDataList> prepareTransaction(ApiCommand::Value cmd, const ec2::ApiCameraAttributesDataList& cameraAttributesList);
        QnTransaction<ApiIdData> prepareTransaction( ApiCommand::Value command, const QnUuid& id );
    };
}
