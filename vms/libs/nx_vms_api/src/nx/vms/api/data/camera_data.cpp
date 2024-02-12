// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "camera_data.h"

#include <nx/fusion/model_functions.h>
#include <nx/utils/log/assert.h>

namespace nx {
namespace vms {
namespace api {

const nx::Uuid CameraData::kDesktopCameraTypeId("{1657647e-f6e4-bc39-d5e8-563c93cb5e1c}");
const nx::Uuid CameraData::kVirtualCameraTypeId("{f7f5ab66-7075-4d0b-a0b2-75e2fdd079a4}");

void CameraData::fillId()
{
    if (!physicalId.isEmpty())
        id = physicalIdToId(physicalId);
    else
        id = nx::Uuid();
}

nx::Uuid CameraData::physicalIdToId(const QString& physicalId)
{
    NX_ASSERT(!physicalId.isEmpty());
    return nx::Uuid::fromArbitraryData(physicalId.toUtf8());
}

QN_FUSION_ADAPT_STRUCT_FUNCTIONS(
    CameraData, (ubjson)(xml)(json)(sql_record)(csv_record), CameraData_Fields)


} // namespace api
} // namespace vms
} // namespace nx
