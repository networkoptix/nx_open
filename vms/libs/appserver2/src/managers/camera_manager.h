// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <nx/network/rest/user_access_data.h>
#include <nx_ec/managers/abstract_camera_manager.h>
#include <rest/request_type_wrappers.h>
#include <transaction/transaction.h>

namespace ec2 {

template<class QueryProcessorType>
class QnCameraManager: public AbstractCameraManager
{
public:
    QnCameraManager(QueryProcessorType* queryProcessor, const nx::network::rest::UserSession& userSession);

    virtual int getCameras(
        Handler<nx::vms::api::CameraDataList> handler,
        nx::utils::AsyncHandlerExecutor handlerExecutor = {}) override;

    virtual int getCamerasEx(
        Handler<nx::vms::api::CameraDataExList> handler,
        nx::utils::AsyncHandlerExecutor handlerExecutor = {}) override;

    virtual int addCamera(
        const nx::vms::api::CameraData& data,
        Handler<> handler,
        nx::utils::AsyncHandlerExecutor handlerExecutor = {}) override;

    virtual int addCameras(
        const nx::vms::api::CameraDataList& dataList,
        Handler<> handler,
        nx::utils::AsyncHandlerExecutor handlerExecutor = {}) override;

    virtual int remove(
        const nx::Uuid& id,
        Handler<> handler,
        nx::utils::AsyncHandlerExecutor handlerExecutor = {}) override;

    virtual int getServerFootageData(
        Handler<nx::vms::api::ServerFootageDataList> handler,
        nx::utils::AsyncHandlerExecutor handlerExecutor = {}) override;

    virtual int setServerFootageData(
        const nx::Uuid& serverGuid,
        const std::vector<nx::Uuid>& cameraIds,
        Handler<> handler,
        nx::utils::AsyncHandlerExecutor handlerExecutor = {}) override;

    virtual int getUserAttributes(
        Handler<nx::vms::api::CameraAttributesDataList> handler,
        nx::utils::AsyncHandlerExecutor handlerExecutor = {}) override;

    virtual int saveUserAttributes(
        const nx::vms::api::CameraAttributesDataList& dataList,
        Handler<> handler,
        nx::utils::AsyncHandlerExecutor handlerExecutor = {}) override;

    virtual int addHardwareIdMapping(
        const nx::vms::api::HardwareIdMapping& hardwareIdMapping,
        Handler<> handler,
        nx::utils::AsyncHandlerExecutor handlerExecutor = {}) override;

    virtual int removeHardwareIdMapping(
        const nx::Uuid& id,
        Handler<> handler,
        nx::utils::AsyncHandlerExecutor handlerExecutor = {}) override;

    virtual int getHardwareIdMappings(
        Handler<nx::vms::api::HardwareIdMappingList> handler,
        nx::utils::AsyncHandlerExecutor handlerExecutor = {}) override;

private:
    decltype(auto) processor() { return m_queryProcessor->getAccess(m_userSession); }

private:
    QueryProcessorType* const m_queryProcessor;
    nx::network::rest::UserSession m_userSession;
};

template<class QueryProcessorType>
QnCameraManager<QueryProcessorType>::QnCameraManager(
    QueryProcessorType* queryProcessor, const nx::network::rest::UserSession& userSession)
    :
    m_queryProcessor(queryProcessor),
    m_userSession(userSession)
{
}

template<class QueryProcessorType>
int QnCameraManager<QueryProcessorType>::getCameras(
    Handler<nx::vms::api::CameraDataList> handler,
    nx::utils::AsyncHandlerExecutor handlerExecutor)
{
    const int requestId = generateRequestID();
    processor().template processQueryAsync<nx::Uuid, nx::vms::api::CameraDataList>(
        ApiCommand::getCameras,
        nx::Uuid(),
        [requestId, handler = handlerExecutor.bind(std::move(handler))](auto&&... args) mutable
        {
            handler(requestId, std::move(args)...);
        });
    return requestId;
}

template<class QueryProcessorType>
int QnCameraManager<QueryProcessorType>::getCamerasEx(
    Handler<nx::vms::api::CameraDataExList> handler,
    nx::utils::AsyncHandlerExecutor handlerExecutor)
{
    const int requestId = generateRequestID();
    processor().template processQueryAsync<QnCameraDataExQuery, nx::vms::api::CameraDataExList>(
        ApiCommand::getCamerasEx,
        QnCameraDataExQuery(),
        [requestId, handler = handlerExecutor.bind(std::move(handler))](auto&&... args) mutable
        {
            handler(requestId, std::move(args)...);
        });
    return requestId;
}

template<class QueryProcessorType>
int QnCameraManager<QueryProcessorType>::addCamera(
    const nx::vms::api::CameraData& data, Handler<> handler,
    nx::utils::AsyncHandlerExecutor handlerExecutor)
{
    const int requestId = generateRequestID();
    processor().processUpdateAsync(
        ApiCommand::saveCamera,
        data,
        [requestId, handler = handlerExecutor.bind(std::move(handler))](auto&&... args) mutable
        {
            handler(requestId, std::move(args)...);
        });
    return requestId;
}

template<class QueryProcessorType>
int QnCameraManager<QueryProcessorType>::addCameras(
    const nx::vms::api::CameraDataList& dataList,
    Handler<> handler,
    nx::utils::AsyncHandlerExecutor handlerExecutor)
{
    for (const auto& data: dataList)
    {
        if (data.id.isNull())
        {
            NX_ASSERT(0, "Only update operation is supported");
            return INVALID_REQ_ID;
        }
    }

    const int requestId = generateRequestID();
    processor().processUpdateAsync(
        ApiCommand::saveCameras,
        dataList,
        [requestId, handler = handlerExecutor.bind(std::move(handler))](auto&&... args) mutable
        {
            handler(requestId, std::move(args)...);
        });
    return requestId;
}

template<class QueryProcessorType>
int QnCameraManager<QueryProcessorType>::remove(
    const nx::Uuid& id,
    Handler<> handler,
    nx::utils::AsyncHandlerExecutor handlerExecutor)
{
    const int requestId = generateRequestID();
    processor().processUpdateAsync(
        ApiCommand::removeCamera,
        nx::vms::api::IdData(id),
        [requestId, handler = handlerExecutor.bind(std::move(handler))](auto&&... args) mutable
        {
            handler(requestId, std::move(args)...);
        });
    return requestId;
}

template<class QueryProcessorType>
int QnCameraManager<QueryProcessorType>::getServerFootageData(
    Handler<nx::vms::api::ServerFootageDataList> handler,
    nx::utils::AsyncHandlerExecutor handlerExecutor)
{
    const int requestId = generateRequestID();
    processor().template processQueryAsync<std::nullptr_t, nx::vms::api::ServerFootageDataList>(
        ApiCommand::getCameraHistoryItems,
        nullptr,
        [requestId, handler = handlerExecutor.bind(std::move(handler))](auto&&... args) mutable
        {
            handler(requestId, std::move(args)...);
        });
    return requestId;
}

template<class QueryProcessorType>
int QnCameraManager<QueryProcessorType>::setServerFootageData(
    const nx::Uuid& serverGuid,
    const std::vector<nx::Uuid>& cameraIds,
    Handler<> handler,
    nx::utils::AsyncHandlerExecutor handlerExecutor)
{
    const int requestId = generateRequestID();
    processor().processUpdateAsync(ApiCommand::addCameraHistoryItem,
        nx::vms::api::ServerFootageData(serverGuid, cameraIds),
        [requestId, handler = handlerExecutor.bind(std::move(handler))](auto&&... args) mutable
        {
            handler(requestId, std::move(args)...);
        });
    return requestId;
}

template<class QueryProcessorType>
int QnCameraManager<QueryProcessorType>::getUserAttributes(
    Handler<nx::vms::api::CameraAttributesDataList> handler,
    nx::utils::AsyncHandlerExecutor handlerExecutor)
{
    const int requestId = generateRequestID();
    processor().template processQueryAsync<nx::Uuid, nx::vms::api::CameraAttributesDataList>(
        ApiCommand::getCameraUserAttributesList,
        nx::Uuid(),
        [requestId, handler = handlerExecutor.bind(std::move(handler))](auto&&... args) mutable
        {
            handler(requestId, std::move(args)...);
        });
    return requestId;
}

template<class QueryProcessorType>
int QnCameraManager<QueryProcessorType>::saveUserAttributes(
    const nx::vms::api::CameraAttributesDataList& dataList,
    Handler<> handler,
    nx::utils::AsyncHandlerExecutor handlerExecutor)
{
    const int requestId = generateRequestID();
    processor().processUpdateAsync(
        ApiCommand::saveCameraUserAttributesList,
        dataList,
        [requestId, handler = handlerExecutor.bind(std::move(handler))](auto&&... args) mutable
        {
            handler(requestId, std::move(args)...);
        });
    return requestId;
}

template<class QueryProcessorType>
int QnCameraManager<QueryProcessorType>::addHardwareIdMapping(
    const nx::vms::api::HardwareIdMapping& hardwareIdMapping,
    Handler<> handler,
    nx::utils::AsyncHandlerExecutor handlerExecutor)
{
    const int requestId = generateRequestID();
    processor().processUpdateAsync(
        ApiCommand::addHardwareIdMapping,
        hardwareIdMapping,
        [requestId, handler = handlerExecutor.bind(std::move(handler))](auto&&... args) mutable
        {
            handler(requestId, std::move(args)...);
        });
    return requestId;
}

template<class QueryProcessorType>
int QnCameraManager<QueryProcessorType>::removeHardwareIdMapping(
    const nx::Uuid& id,
    Handler<> handler,
    nx::utils::AsyncHandlerExecutor handlerExecutor)
{
    const int requestId = generateRequestID();
    processor().processUpdateAsync(
        ApiCommand::removeHardwareIdMapping,
        nx::vms::api::IdData(id),
        [requestId, handler = handlerExecutor.bind(std::move(handler))](auto&&... args) mutable
        {
            handler(requestId, std::move(args)...);
        });
    return requestId;
}

template<class QueryProcessorType>
int QnCameraManager<QueryProcessorType>::getHardwareIdMappings(
    Handler<nx::vms::api::HardwareIdMappingList> handler,
    nx::utils::AsyncHandlerExecutor handlerExecutor)
{
    const int requestId = generateRequestID();
    processor().template processQueryAsync<std::nullptr_t, nx::vms::api::HardwareIdMappingList>(
        ApiCommand::getHardwareIdMappings,
        nullptr,
        [requestId, handler = handlerExecutor.bind(std::move(handler))](auto&&... args) mutable
        {
            handler(requestId, std::move(args)...);
        });
    return requestId;
}

} // namespace ec2
