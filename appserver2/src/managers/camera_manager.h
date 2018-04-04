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


    template<class QueryProcessorType>
    QnCameraManager<QueryProcessorType>::QnCameraManager(QueryProcessorType* const queryProcessor, const Qn::UserAccessData &userAccessData)
        :
        m_queryProcessor(queryProcessor),
        m_userAccessData(userAccessData)
    {
    }

    template<class QueryProcessorType>
    int QnCameraManager<QueryProcessorType>::getCameras(impl::GetCamerasHandlerPtr handler)
    {
        const int reqID = generateRequestID();

        auto queryDoneHandler = [reqID, handler, this](ErrorCode errorCode, const ApiCameraDataList& cameras)
        {
            handler->done(reqID, errorCode, cameras);
        };
        m_queryProcessor->getAccess(m_userAccessData).template processQueryAsync<QnUuid, ApiCameraDataList, decltype(queryDoneHandler)>
            (ApiCommand::getCameras, QnUuid(), queryDoneHandler);
        return reqID;
    }

    template<class QueryProcessorType>
    int QnCameraManager<QueryProcessorType>::getCamerasEx(impl::GetCamerasExHandlerPtr handler)
    {
        const int reqID = generateRequestID();

        auto queryDoneHandler = [reqID, handler, this](ErrorCode errorCode, const ApiCameraDataExList& cameras)
        {
            handler->done(reqID, errorCode, cameras);
        };
        m_queryProcessor->getAccess(m_userAccessData).template processQueryAsync<QnUuid, ApiCameraDataExList, decltype(queryDoneHandler)>
            (ApiCommand::getCamerasEx, QnUuid(), queryDoneHandler);
        return reqID;
    }

    template<class QueryProcessorType>
    int QnCameraManager<QueryProcessorType>::addCamera(const ec2::ApiCameraData& camera, impl::SimpleHandlerPtr handler)
    {
        const int reqID = generateRequestID();
        m_queryProcessor->getAccess(m_userAccessData).processUpdateAsync(ApiCommand::saveCamera, camera, [handler, reqID](ec2::ErrorCode errorCode)
        {
            handler->done(reqID, errorCode);
        });
        return reqID;
    }

    template<class QueryProcessorType>
    int QnCameraManager<QueryProcessorType>::save(const ec2::ApiCameraDataList& cameras, impl::SimpleHandlerPtr handler)
    {
        for (const auto& camera : cameras)
        {
            if (camera.id.isNull())
            {
                NX_ASSERT(0, "Only update operation is supported", Q_FUNC_INFO);
                return INVALID_REQ_ID;
            }
        }

        const int reqID = generateRequestID();
        m_queryProcessor->getAccess(m_userAccessData).processUpdateAsync(ApiCommand::saveCameras, cameras, [handler, reqID](ec2::ErrorCode errorCode)
        {
            handler->done(reqID, errorCode);
        });
        return reqID;
    }

    template<class QueryProcessorType>
    int QnCameraManager<QueryProcessorType>::remove(const QnUuid& id, impl::SimpleHandlerPtr handler)
    {
        const int reqID = generateRequestID();
        m_queryProcessor->getAccess(m_userAccessData).processUpdateAsync(
            ApiCommand::removeCamera, ApiIdData(id),
            [handler, reqID](ec2::ErrorCode errorCode)
        {
            handler->done(reqID, errorCode);
        });
        return reqID;
    }

    template<class QueryProcessorType>
    int QnCameraManager<QueryProcessorType>::setServerFootageData(const QnUuid& serverGuid, const std::vector<QnUuid>& cameras, impl::SimpleHandlerPtr handler)
    {
        ApiServerFootageData data(serverGuid, cameras);
        const int reqID = generateRequestID();
        m_queryProcessor->getAccess(m_userAccessData).processUpdateAsync(ApiCommand::addCameraHistoryItem, data, [handler, reqID](ec2::ErrorCode errorCode)
        {
            handler->done(reqID, errorCode);
        });
        return reqID;
    }


    template<class QueryProcessorType>
    int QnCameraManager<QueryProcessorType>::getServerFootageData(impl::GetCamerasHistoryHandlerPtr handler)
    {
        const int reqID = generateRequestID();

        auto queryDoneHandler = [reqID, handler](ErrorCode errorCode, const ApiServerFootageDataList& cameraHistory) {
            handler->done(reqID, errorCode, cameraHistory);
        };
        m_queryProcessor->getAccess(m_userAccessData).template processQueryAsync<std::nullptr_t, ApiServerFootageDataList, decltype(queryDoneHandler)>(
            ApiCommand::getCameraHistoryItems, nullptr, queryDoneHandler);
        return reqID;
    }


    template<class QueryProcessorType>
    int QnCameraManager<QueryProcessorType>::saveUserAttributes(const ec2::ApiCameraAttributesDataList& cameraAttributes, impl::SimpleHandlerPtr handler)
    {
        const int reqID = generateRequestID();
        m_queryProcessor->getAccess(m_userAccessData).processUpdateAsync(ApiCommand::saveCameraUserAttributesList, cameraAttributes, [handler, reqID](ec2::ErrorCode errorCode)
        {
            handler->done(reqID, errorCode);
        });
        return reqID;
    }

    template<class QueryProcessorType>
    int QnCameraManager<QueryProcessorType>::getUserAttributes(impl::GetCameraUserAttributesHandlerPtr handler)
    {
        const int reqID = generateRequestID();

        auto queryDoneHandler = [reqID, handler, this](ErrorCode errorCode, const ApiCameraAttributesDataList& cameraUserAttributesList) {
            handler->done(reqID, errorCode, cameraUserAttributesList);
        };
        m_queryProcessor->getAccess(m_userAccessData).template processQueryAsync<QnUuid, ApiCameraAttributesDataList, decltype(queryDoneHandler)>
            (ApiCommand::getCameraUserAttributesList, QnUuid(), queryDoneHandler);
        return reqID;
    }

} // namespace ec2
