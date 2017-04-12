#pragma once

#include <nx_ec/ec_api_fwd.h>
#include <nx_ec/data/api_camera_data.h>
#include <nx_ec/data/api_camera_data_ex.h>
#include <nx_ec/data/api_camera_history_data.h>
#include <nx_ec/data/api_camera_attributes_data.h>
#include <nx_ec/impl/ec_api_impl.h>
#include <nx_ec/impl/sync_handler.h>

namespace ec2
{

class AbstractCameraNotificationManager : public QObject
{
    Q_OBJECT
public:
signals:
    void addedOrUpdated(const ec2::ApiCameraData& camera, NotificationSource source);
    void cameraHistoryChanged(const ec2::ApiServerFootageData& cameraHistory);
    void removed(const QnUuid& id);

    void userAttributesChanged(const ec2::ApiCameraAttributesData& attributes);
    void userAttributesRemoved(const QnUuid& id);
};

typedef std::shared_ptr<AbstractCameraNotificationManager> AbstractCameraNotificationManagerPtr;

    /*!
    \note All methods are asynchronous if other not specified
    */
    class AbstractCameraManager
    {
    public:
        virtual ~AbstractCameraManager() {}

        /*!
        \param handler Functor with params: (ErrorCode, const ec2::ApiCameraDataList& cameras)
        */
        template<class TargetType, class HandlerType>
        int getCameras(TargetType* target, HandlerType handler)
        {
            return getCameras(std::static_pointer_cast<impl::GetCamerasHandler>(
                std::make_shared<impl::CustomGetCamerasHandler<TargetType, HandlerType>>(target, handler)));
        }

        ErrorCode getCamerasSync(ec2::ApiCameraDataList* const cameraList)
        {
            return impl::doSyncCall<impl::GetCamerasHandler>([this](impl::GetCamerasHandlerPtr handler)
            {
                this->getCameras(handler);
            }, cameraList);
        }

        ErrorCode getCamerasExSync(ec2::ApiCameraDataExList* const cameraList)
        {
            return impl::doSyncCall<impl::GetCamerasExHandler>([this](impl::GetCamerasExHandlerPtr handler)
            {
                this->getCamerasEx(handler);
            }, cameraList);
        }

        /*!
        \param handler Functor with params: (ErrorCode)
        */
        template<class TargetType, class HandlerType> int addCamera(const ec2::ApiCameraData& camera, TargetType* target, HandlerType handler)
        {
            return addCamera(camera, std::static_pointer_cast<impl::SimpleHandler>(std::make_shared<impl::CustomSimpleHandler<TargetType, HandlerType>>(target, handler)));
        }

        ErrorCode addCameraSync(const ec2::ApiCameraData& camera)
        {
            return impl::doSyncCall<impl::SimpleHandler>([this, camera](impl::SimpleHandlerPtr handler)
            {
                this->addCamera(camera, handler);
            });
        }

        ErrorCode addCamerasSync(const ec2::ApiCameraDataList& cameras)
        {
            return impl::doSyncCall<impl::SimpleHandler>([this, cameras](impl::SimpleHandlerPtr handler)
            {
                this->save(cameras, handler);
            });
        }

        /*!
        \param handler Functor with params: (ErrorCode)
        */
        template<class TargetType, class HandlerType> int save(const ec2::ApiCameraDataList& cameras, TargetType* target, HandlerType handler)
        {
            return save(cameras, std::static_pointer_cast<impl::SimpleHandler>(
                std::make_shared<impl::CustomSimpleHandler<TargetType, HandlerType>>(target, handler)));
        }

        //!Remove camera. Camera user attributes are not removed by this method!
        /*!
        \param handler Functor with params: (ErrorCode)
        */
        template<class TargetType, class HandlerType>
        int remove(const QnUuid& id, TargetType* target, HandlerType handler)
        {
            return remove(id, std::static_pointer_cast<impl::SimpleHandler>(
                std::make_shared<impl::CustomSimpleHandler<TargetType, HandlerType>>(target, handler)));
        }

        /*!
        \param handler Functor with params: (ErrorCode, const ApiServerFootageDataList& cameras)
        */
        template<class TargetType, class HandlerType>
        int getServerFootageData(TargetType* target, HandlerType handler)
        {
            return getServerFootageData(std::static_pointer_cast<impl::GetCamerasHistoryHandler>(
                std::make_shared<impl::CustomGetCamerasHistoryHandler<TargetType, HandlerType>>(target, handler)));
        }

        ErrorCode getServerFootageDataSync(ApiServerFootageDataList* const serverFootageData)
        {
            return impl::doSyncCall<impl::GetCamerasHistoryHandler>([this](impl::GetCamerasHistoryHandlerPtr handler)
            {
                this->getServerFootageData(handler);
            }, serverFootageData);
        }

        /*!
        \param handler Functor with params: (ErrorCode)
        */
        template<class TargetType, class HandlerType>
        int setServerFootageData(const QnUuid& serverGuid, const std::vector<QnUuid>& cameras, TargetType* target, HandlerType handler)
        {
            return setServerFootageData(serverGuid, cameras, std::static_pointer_cast<impl::SimpleHandler>(
                std::make_shared<impl::CustomSimpleHandler<TargetType, HandlerType>>(target, handler)));
        }

        ErrorCode setServerFootageDataSync(const QnUuid& serverGuid, const std::vector<QnUuid>& cameras)
        {
            return impl::doSyncCall<impl::SimpleHandler>([this, serverGuid, cameras](impl::SimpleHandlerPtr handler)
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
            return getUserAttributes(std::static_pointer_cast<impl::GetCameraUserAttributesHandler>(
                std::make_shared<impl::CustomGetCameraUserAttributesHandler<TargetType, HandlerType>>(target, handler)));
        }

        ErrorCode getUserAttributesSync(ec2::ApiCameraAttributesDataList* const attributesList)
        {
            return impl::doSyncCall<impl::GetCameraUserAttributesHandler>([this](impl::GetCameraUserAttributesHandlerPtr handler)
            {
                this->getUserAttributes(handler);
            }, attributesList);
        }

        /*!
        \param handler Functor with params: (ErrorCode)
        */
        template<class TargetType, class HandlerType>
        int saveUserAttributes(const ec2::ApiCameraAttributesDataList& attributes, TargetType* target, HandlerType handler)
        {
            return saveUserAttributes(attributes, std::static_pointer_cast<impl::SimpleHandler>(
                std::make_shared<impl::CustomSimpleHandler<TargetType, HandlerType>>(target, handler)));
        }

        ErrorCode saveUserAttributesSync(const ec2::ApiCameraAttributesDataList& attributes)
        {
            return impl::doSyncCall<impl::SimpleHandler>([this, attributes](impl::SimpleHandlerPtr handler)
            {
                this->saveUserAttributes(attributes, handler);
            });
        }


    protected:
        virtual int getCameras(impl::GetCamerasHandlerPtr handler) = 0;
        virtual int getCamerasEx(impl::GetCamerasExHandlerPtr handler) = 0;
        virtual int addCamera(const ec2::ApiCameraData& camera, impl::SimpleHandlerPtr handler) = 0;
        virtual int save(const ec2::ApiCameraDataList& cameras, impl::SimpleHandlerPtr handler) = 0;
        virtual int remove(const QnUuid& id, impl::SimpleHandlerPtr handler) = 0;

        virtual int setServerFootageData(const QnUuid& serverGuid, const std::vector<QnUuid>& cameras, impl::SimpleHandlerPtr handler) = 0;
        virtual int getServerFootageData(impl::GetCamerasHistoryHandlerPtr handler) = 0;
        virtual int saveUserAttributes(const ec2::ApiCameraAttributesDataList& cameras, impl::SimpleHandlerPtr handler) = 0;
        virtual int getUserAttributes(impl::GetCameraUserAttributesHandlerPtr handler) = 0;
    };

}
