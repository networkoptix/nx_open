#pragma once

#include "api_globals.h"
#include "api_resource_data.h"
#include <utils/common/id.h>

namespace ec2 {

struct ApiCameraData: ApiResourceData
{
    ApiCameraData(): manuallyAdded(false), statusFlags(0) {}

    /**
     * See fillId() in ApiIdData
     */
    void fillId()
    {
        // ATTENTION: This logic is similar to the one in
        // QnSecurityCamResource::makeCameraIdFromUniqueId().
        if (!physicalId.isEmpty())
            id = guidFromArbitraryData(physicalId.toUtf8());
        else
            id = QnUuid();
    }

    QnLatin1Array mac;
    QString physicalId;
    bool manuallyAdded;
    QString model;
    QString groupId;
    QString groupName;
    Qn::CameraStatusFlags statusFlags;
    QString vendor;
};
#define ApiCameraData_Fields ApiResourceData_Fields \
    (mac)(physicalId)(manuallyAdded)(model)(groupId)(groupName)(statusFlags)(vendor)

} // namespace ec2
