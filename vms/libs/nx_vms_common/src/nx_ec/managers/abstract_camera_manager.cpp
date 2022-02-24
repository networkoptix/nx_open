// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "abstract_camera_manager.h"

#include "../detail/call_sync.h"

namespace ec2 {

ErrorCode AbstractCameraManager::getCamerasSync(nx::vms::api::CameraDataList* outDataList)
{
    return detail::callSync(
        [&](auto handler)
        {
            getCameras(std::move(handler));
        },
        outDataList);
}

ErrorCode AbstractCameraManager::getCamerasExSync(nx::vms::api::CameraDataExList* outDataList)
{
    return detail::callSync(
        [&](auto handler)
        {
            getCamerasEx(std::move(handler));
        },
        outDataList);
}

ErrorCode AbstractCameraManager::addCameraSync(const nx::vms::api::CameraData& data)
{
    return detail::callSync(
        [&](auto handler)
        {
            addCamera(data, std::move(handler));
        });
}

ErrorCode AbstractCameraManager::addCamerasSync(const nx::vms::api::CameraDataList& dataList)
{
    return detail::callSync(
        [&](auto handler)
        {
            addCameras(dataList, std::move(handler));
        });
}

ErrorCode AbstractCameraManager::removeSync(const QnUuid& id)
{
    return detail::callSync(
        [&](auto handler)
        {
            remove(id, std::move(handler));
        });
}

ErrorCode AbstractCameraManager::getServerFootageDataSync(
    nx::vms::api::ServerFootageDataList* outDataList)
{
    return detail::callSync(
        [&](auto handler)
        {
            getServerFootageData(std::move(handler));
        },
        outDataList);
}

ErrorCode AbstractCameraManager::setServerFootageDataSync(
    const QnUuid& serverGuid, const std::vector<QnUuid>& cameraIds)
{
    return detail::callSync(
        [&](auto handler)
        {
            setServerFootageData(serverGuid, cameraIds, std::move(handler));
        });
}

ErrorCode AbstractCameraManager::getUserAttributesSync(
    nx::vms::api::CameraAttributesDataList* outDataList)
{
    return detail::callSync(
        [&](auto handler)
        {
            getUserAttributes(std::move(handler));
        },
        outDataList);
}

ErrorCode AbstractCameraManager::saveUserAttributesSync(
    const nx::vms::api::CameraAttributesDataList& dataList)
{
    return detail::callSync(
        [&](auto handler)
        {
            saveUserAttributes(dataList, std::move(handler));
        });
}

} // namespace ec2
