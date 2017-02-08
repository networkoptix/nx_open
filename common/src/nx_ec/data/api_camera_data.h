#pragma once

#include "api_globals.h"
#include "api_resource_data.h"
#include <utils/common/id.h>
#include <nx/utils/log/assert.h>

namespace ec2 {

struct ApiCameraData: ApiResourceData
{
    ApiCameraData(): manuallyAdded(false), statusFlags(0) {}

    /**
     * See fillId() in ApiIdData
     */
    void fillId()
    {
        if (!physicalId.isEmpty())
            id = physicalIdToId(physicalId);
        else
            id = QnUuid();
    }

    static QnUuid physicalIdToId(const QString& physicalId)
    {
        NX_ASSERT(!physicalId.isEmpty());
        return guidFromArbitraryData(physicalId.toUtf8());
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
