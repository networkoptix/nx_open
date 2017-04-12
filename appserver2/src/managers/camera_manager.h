#pragma once

#include <transaction/transaction.h>
#include <nx_ec/managers/abstract_camera_manager.h>

namespace ec2
{
    class QnCameraNotificationManager: public AbstractCameraNotificationManager
    {
    public:
        QnCameraNotificationManager();

        void triggerNotification(const QnTransaction<ApiCameraData>& tran, NotificationSource source);
        void triggerNotification(const QnTransaction<ApiCameraDataList>& tran, NotificationSource source);
        void triggerNotification(const QnTransaction<ApiCameraAttributesData>& tran, NotificationSource source);
        void triggerNotification(const QnTransaction<ApiCameraAttributesDataList>& tran, NotificationSource source);
        void triggerNotification(const QnTransaction<ApiIdData>& tran, NotificationSource source);
        void triggerNotification(const QnTransaction<ApiServerFootageData>& tran, NotificationSource source);
    };

    typedef std::shared_ptr<QnCameraNotificationManager> QnCameraNotificationManagerPtr;

    template<class QueryProcessorType>
    class QnCameraManager: public AbstractCameraManager
    {
    public:
        QnCameraManager(QueryProcessorType* const queryProcessor, const Qn::UserAccessData &userAccessData);

        //!Implementation of AbstractCameraManager::getCameras
        virtual int getCameras(impl::GetCamerasHandlerPtr handler) override;
        virtual int getCamerasEx(impl::GetCamerasExHandlerPtr handler) override;
        //!Implementation of AbstractCameraManager::addCamera
        virtual int addCamera(const ec2::ApiCameraData&, impl::SimpleHandlerPtr handler) override;
        //!Implementation of AbstractCameraManager::save
        virtual int save(const ec2::ApiCameraDataList& cameras, impl::SimpleHandlerPtr handler) override;
        //!Implementation of AbstractCameraManager::remove
        virtual int remove(const QnUuid& id, impl::SimpleHandlerPtr handler) override;

        //!Implementation of AbstractCameraManager::ApiServerFootageData
        virtual int setServerFootageData(const QnUuid& serverGuid, const std::vector<QnUuid>& cameras, impl::SimpleHandlerPtr handler) override;
        //!Implementation of AbstractCameraManager::getCameraHistoryList
        virtual int getServerFootageData(impl::GetCamerasHistoryHandlerPtr handler) override;
        //!Implementation of AbstractCameraManager::saveUserAttributes
        virtual int saveUserAttributes(const ec2::ApiCameraAttributesDataList& cameraAttributes, impl::SimpleHandlerPtr handler) override;
        //!Implementation of AbstractCameraManager::getUserAttributes
        virtual int getUserAttributes(impl::GetCameraUserAttributesHandlerPtr handler) override;

    private:
        QueryProcessorType* const m_queryProcessor;
        Qn::UserAccessData m_userAccessData;
    };
}
