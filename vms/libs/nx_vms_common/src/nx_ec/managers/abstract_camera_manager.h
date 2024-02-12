// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <vector>

#include <QtCore/QObject>

#include <nx/utils/async_handler_executor.h>
#include <nx/utils/uuid.h>
#include <nx/vms/api/data/camera_attributes_data.h>
#include <nx/vms/api/data/camera_data.h>
#include <nx/vms/api/data/camera_data_ex.h>
#include <nx/vms/api/data/camera_history_data.h>
#include <nx/vms/api/data/hardware_id_mapping.h>

#include "../ec_api_common.h"

namespace ec2 {

class NX_VMS_COMMON_API AbstractCameraNotificationManager: public QObject
{
    Q_OBJECT

signals:
    void addedOrUpdated(const nx::vms::api::CameraData& camera, ec2::NotificationSource source);
    void cameraHistoryChanged(const nx::vms::api::ServerFootageData& cameraHistory);
    void removed(const nx::Uuid& id, ec2::NotificationSource source);

    void userAttributesChanged(const nx::vms::api::CameraAttributesData& attributes);
    void userAttributesRemoved(const nx::Uuid& id);

    void hardwareIdMappingAdded(const nx::vms::api::HardwareIdMapping& hardwareIdMapping);
    void hardwareIdMappingRemoved(const nx::Uuid& id);
};

/*!
\note All methods are asynchronous if other not specified
*/
class NX_VMS_COMMON_API AbstractCameraManager
{
public:
    virtual ~AbstractCameraManager() = default;

    virtual int getCameras(
        Handler<nx::vms::api::CameraDataList> handler,
        nx::utils::AsyncHandlerExecutor handlerExecutor = {}) = 0;

    ErrorCode getCamerasSync(nx::vms::api::CameraDataList* outDataList);

    virtual int getCamerasEx(
        Handler<nx::vms::api::CameraDataExList> handler,
        nx::utils::AsyncHandlerExecutor handlerExecutor = {}) = 0;

    ErrorCode getCamerasExSync(nx::vms::api::CameraDataExList* outDataList);

    virtual int addCamera(
        const nx::vms::api::CameraData& data,
        Handler<> handler,
        nx::utils::AsyncHandlerExecutor handlerExecutor = {}) = 0;

    virtual int addHardwareIdMapping(
        const nx::vms::api::HardwareIdMapping& hardwareIdMapping,
        Handler<> handler,
        nx::utils::AsyncHandlerExecutor handlerExecutor = {}) = 0;

    ErrorCode addHardwareIdMappingSync(const nx::vms::api::HardwareIdMapping& hardwareIdMapping);

    virtual int removeHardwareIdMapping(
        const nx::Uuid& id,
        Handler<> handler,
        nx::utils::AsyncHandlerExecutor handlerExecutor = {}) = 0;

    ErrorCode removeHardwareIdMappingSync(const nx::Uuid& id);

    virtual int getHardwareIdMappings(
        Handler<nx::vms::api::HardwareIdMappingList> handler,
        nx::utils::AsyncHandlerExecutor handlerExecutor = {}) = 0;

    ErrorCode getHardwareIdMappingsSync(nx::vms::api::HardwareIdMappingList* outHardwareIdMappings);

    ErrorCode addCameraSync(const nx::vms::api::CameraData& data);

    virtual int addCameras(
        const nx::vms::api::CameraDataList& dataList,
        Handler<> handler,
        nx::utils::AsyncHandlerExecutor handlerExecutor = {}) = 0;

    ErrorCode addCamerasSync(const nx::vms::api::CameraDataList& dataList);

    virtual int remove(
        const nx::Uuid& id,
        Handler<> handler,
        nx::utils::AsyncHandlerExecutor handlerExecutor = {}) = 0;

    ErrorCode removeSync(const nx::Uuid& id);

    virtual int getServerFootageData(
        Handler<nx::vms::api::ServerFootageDataList> handler,
        nx::utils::AsyncHandlerExecutor handlerExecutor = {}) = 0;

    Result getServerFootageDataSync(nx::vms::api::ServerFootageDataList* outDataList);

    virtual int setServerFootageData(
        const nx::Uuid& serverGuid,
        const std::vector<nx::Uuid>& cameraIds,
        Handler<> handler,
        nx::utils::AsyncHandlerExecutor handlerExecutor = {}) = 0;

    ErrorCode setServerFootageDataSync(
        const nx::Uuid& serverGuid, const std::vector<nx::Uuid>& cameraIds);

    virtual int getUserAttributes(
        Handler<nx::vms::api::CameraAttributesDataList> handler,
        nx::utils::AsyncHandlerExecutor handlerExecutor = {}) = 0;

    ErrorCode getUserAttributesSync(nx::vms::api::CameraAttributesDataList* outDataList);

    virtual int saveUserAttributes(
        const nx::vms::api::CameraAttributesDataList& dataList,
        Handler<> handler,
        nx::utils::AsyncHandlerExecutor handlerExecutor = {}) = 0;

    ErrorCode saveUserAttributesSync(const nx::vms::api::CameraAttributesDataList& dataList);
};

} // namespace ec2
