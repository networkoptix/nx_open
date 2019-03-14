#pragma once

#include "resource_data.h"

#include <QtCore/QString>

#include <nx/utils/latin1_array.h>

namespace nx {
namespace vms {
namespace api {

struct NX_VMS_API CameraData: ResourceData
{
    /**
     * See fillId() in IdData
     */
    void fillId();

    static QnUuid physicalIdToId(const QString& physicalId);

    static const QnUuid kDesktopCameraTypeId;
    static const QnUuid kWearableCameraTypeId;

    QnLatin1Array mac;
    QString physicalId;
    bool manuallyAdded = false;
    QString model;
    QString groupId;
    QString groupName;
    CameraStatusFlags statusFlags = CSF_NoFlags;
    QString vendor;
};

#define CameraData_Fields ResourceData_Fields \
    (mac)(physicalId)(manuallyAdded)(model)(groupId)(groupName)(statusFlags)(vendor)

} // namespace api
} // namespace vms
} // namespace nx

Q_DECLARE_METATYPE(nx::vms::api::CameraData)
Q_DECLARE_METATYPE(nx::vms::api::CameraDataList)
