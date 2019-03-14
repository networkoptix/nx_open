#pragma once

#include <nx_ec/ec_api_fwd.h>
#include <nx/vms/api/data/camera_data.h>
#include <nx/vms/api/data/camera_data_ex.h>
#include <nx/vms/api/data/camera_history_data.h>
#include <nx/vms/api/data/camera_attributes_data.h>
#include <nx_ec/impl/ec_api_impl.h>
#include <nx_ec/impl/sync_handler.h>

namespace ec2 {

class AbstractCameraNotificationManager: public QObject
{
    Q_OBJECT

signals:
    void addedOrUpdated(const nx::vms::api::CameraData& camera, ec2::NotificationSource source);
    void cameraHistoryChanged(const nx::vms::api::ServerFootageData& cameraHistory);
    void removed(const QnUuid& id);

    void userAttributesChanged(const nx::vms::api::CameraAttributesData& attributes);
    void userAttributesRemoved(const QnUuid& id);
};

typedef std::shared_ptr<AbstractCameraNotificationManager> AbstractCameraNotificationManagerPtr;

/*!
\note All methods are asynchronous if other not specified
*/
class AbstractCameraManager
{
public:
    virtual ~AbstractCameraManager()
    {
    }

    /*!
    \param handler Functor with params: (ErrorCode, const nx::vms::api::CameraDataList& cameras)
    */
    template<class TargetType, class HandlerType>
    int getCameras(TargetType* target, HandlerType handler)
    {
        return getCameras(
            std::static_pointer_cast<impl::GetCamerasHandler>(
                std::make_shared<impl::CustomGetCamerasHandler<TargetType, HandlerType>>(
                    target,
                    handler)));
    }

    ErrorCode getCamerasSync(nx::vms::api::CameraDataList* const cameraList)
    {
        return impl::doSyncCall<impl::GetCamerasHandler>(
            [this](impl::GetCamerasHandlerPtr handler)
            {
                this->getCameras(handler);
            },
            cameraList);
    }

    ErrorCode getCamerasExSync(nx::vms::api::CameraDataExList* const cameraList)
    {
        return impl::doSyncCall<impl::GetCamerasExHandler>(
            [this](impl::GetCamerasExHandlerPtr handler)
            {
                this->getCamerasEx(handler);
            },
            cameraList);
    }

    /*!
    \param handler Functor with params: (ErrorCode)
    */
    template<class TargetType, class HandlerType>
    int addCamera(
        const nx::vms::api::CameraData& camera,
        TargetType* target,
        HandlerType handler)
    {
        return addCamera(
            camera,
            std::static_pointer_cast<impl::SimpleHandler>(
                std::make_shared<impl::CustomSimpleHandler<TargetType, HandlerType>>(
                    target,
                    handler)));
    }

    ErrorCode addCameraSync(const nx::vms::api::CameraData& camera)
    {
        return impl::doSyncCall<impl::SimpleHandler>(
            [this, camera](impl::SimpleHandlerPtr handler)
            {
                this->addCamera(camera, handler);
            });
    }

    ErrorCode addCamerasSync(const nx::vms::api::CameraDataList& cameras)
    {
        return impl::doSyncCall<impl::SimpleHandler>(
            [this, cameras](impl::SimpleHandlerPtr handler)
            {
                this->save(cameras, handler);
            });
    }

    /*!
    \param handler Functor with params: (ErrorCode)
    */
    template<class TargetType, class HandlerType>
    int save(const nx::vms::api::CameraDataList& cameras, TargetType* target, HandlerType handler)
    {
        return save(
            cameras,
            std::static_pointer_cast<impl::SimpleHandler>(
                std::make_shared<impl::CustomSimpleHandler<TargetType, HandlerType>>(
                    target,
                    handler)));
    }

    //!Remove camera. Camera user attributes are not removed by this method!
    /*!
    \param handler Functor with params: (ErrorCode)
    */
    template<class TargetType, class HandlerType>
    int remove(const QnUuid& id, TargetType* target, HandlerType handler)
    {
        return remove(
            id,
            std::static_pointer_cast<impl::SimpleHandler>(
                std::make_shared<impl::CustomSimpleHandler<TargetType, HandlerType>>(
                    target,
                    handler)));
    }

    /*!
    \param handler Functor with params: (ErrorCode, const nx::vms::api::ServerFootageDataList& cameras)
    */
    template<class TargetType, class HandlerType>
    int getServerFootageData(TargetType* target, HandlerType handler)
    {
        return getServerFootageData(
            std::static_pointer_cast<impl::GetCamerasHistoryHandler>(
                std::make_shared<impl::CustomGetCamerasHistoryHandler<TargetType, HandlerType>>(
                    target,
                    handler)));
    }

    ErrorCode getServerFootageDataSync(nx::vms::api::ServerFootageDataList* const serverFootageData)
    {
        return impl::doSyncCall<impl::GetCamerasHistoryHandler>(
            [this](impl::GetCamerasHistoryHandlerPtr handler)
            {
                this->getServerFootageData(handler);
            },
            serverFootageData);
    }

    /*!
    \param handler Functor with params: (ErrorCode)
    */
    template<class TargetType, class HandlerType>
    int setServerFootageData(
        const QnUuid& serverGuid,
        const std::vector<QnUuid>& cameras,
        TargetType* target,
        HandlerType handler)
    {
        return setServerFootageData(
            serverGuid,
            cameras,
            std::static_pointer_cast<impl::SimpleHandler>(
                std::make_shared<impl::CustomSimpleHandler<TargetType, HandlerType>>(
                    target,
                    handler)));
    }

    ErrorCode setServerFootageDataSync(
        const QnUuid& serverGuid,
        const std::vector<QnUuid>& cameras)
    {
        return impl::doSyncCall<impl::SimpleHandler>(
            [this, serverGuid, cameras](impl::SimpleHandlerPtr handler)
            {
                this->setServerFootageData(serverGuid, cameras, handler);
            });
    }

    /*!
    \param handler Functor with params: (ErrorCode, const QnCameraUserAttributesList& cameraUserAttributesList)
    */
    template<class TargetType, class HandlerType>
    int getUserAttributes(TargetType* target, HandlerType handler)
    {
        return getUserAttributes(
            std::static_pointer_cast<impl::GetCameraUserAttributesHandler>(
                std::make_shared<impl::CustomGetCameraUserAttributesHandler<TargetType, HandlerType
                >>(target, handler)));
    }

    ErrorCode getUserAttributesSync(nx::vms::api::CameraAttributesDataList* const attributesList)
    {
        return impl::doSyncCall<impl::GetCameraUserAttributesHandler>(
            [this](impl::GetCameraUserAttributesHandlerPtr handler)
            {
                this->getUserAttributes(handler);
            },
            attributesList);
    }

    /*!
    \param handler Functor with params: (ErrorCode)
    */
    template<class TargetType, class HandlerType>
    int saveUserAttributes(
        const nx::vms::api::CameraAttributesDataList& attributes,
        TargetType* target,
        HandlerType handler)
    {
        return saveUserAttributes(
            attributes,
            std::static_pointer_cast<impl::SimpleHandler>(
                std::make_shared<impl::CustomSimpleHandler<TargetType, HandlerType>>(
                    target,
                    handler)));
    }

    ErrorCode saveUserAttributesSync(const nx::vms::api::CameraAttributesDataList& attributes)
    {
        return impl::doSyncCall<impl::SimpleHandler>(
            [this, attributes](impl::SimpleHandlerPtr handler)
            {
                this->saveUserAttributes(attributes, handler);
            });
    }

protected:
    virtual int getCameras(impl::GetCamerasHandlerPtr handler) = 0;
    virtual int getCamerasEx(impl::GetCamerasExHandlerPtr handler) = 0;
    virtual int addCamera(
        const nx::vms::api::CameraData& camera,
        impl::SimpleHandlerPtr handler) = 0;
    virtual int save(
        const nx::vms::api::CameraDataList& cameras,
        impl::SimpleHandlerPtr handler) = 0;
    virtual int remove(const QnUuid& id, impl::SimpleHandlerPtr handler) = 0;

    virtual int setServerFootageData(
        const QnUuid& serverGuid,
        const std::vector<QnUuid>& cameras,
        impl::SimpleHandlerPtr handler) = 0;
    virtual int getServerFootageData(impl::GetCamerasHistoryHandlerPtr handler) = 0;
    virtual int saveUserAttributes(
        const nx::vms::api::CameraAttributesDataList& cameras,
        impl::SimpleHandlerPtr handler) = 0;
    virtual int getUserAttributes(impl::GetCameraUserAttributesHandlerPtr handler) = 0;
};

}
