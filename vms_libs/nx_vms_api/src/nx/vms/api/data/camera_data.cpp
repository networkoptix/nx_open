#include "camera_data.h"

#include <nx/fusion/model_functions.h>
#include <nx/utils/log/assert.h>

namespace nx {
namespace vms {
namespace api {

const QnUuid CameraData::kDesktopCameraTypeId("{1657647e-f6e4-bc39-d5e8-563c93cb5e1c}");
const QnUuid CameraData::kWearableCameraTypeId("{f7f5ab66-7075-4d0b-a0b2-75e2fdd079a4}");

void CameraData::fillId()
{
    if (!physicalId.isEmpty())
        id = physicalIdToId(physicalId);
    else
        id = QnUuid();
}

QnUuid CameraData::physicalIdToId(const QString& physicalId)
{
    NX_ASSERT(!physicalId.isEmpty());
    return QnUuid::fromArbitraryData(physicalId.toUtf8());
}

QN_FUSION_ADAPT_STRUCT_FUNCTIONS_FOR_TYPES(
    (CameraData),
    (eq)(ubjson)(xml)(json)(sql_record)(csv_record),
    _Fields)

} // namespace api
} // namespace vms
} // namespace nx
