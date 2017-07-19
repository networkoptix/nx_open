#include "camera_id_helper.h"

#include <core/resource_management/resource_pool.h>
#include <core/resource/security_cam_resource.h>
#include <nx/utils/log/log.h>

namespace nx {
namespace camera_id_helper {

void findAllCamerasByFlexibleIds(
    QnResourcePool* resourcePool,
    QnSecurityCamResourceList* cameras,
    const QnRequestParamList& params,
    const QStringList& flexibleIdParamNames)
{
    QStringList allFlexibleIds;
    for (const auto& idParamName: flexibleIdParamNames)
        allFlexibleIds.append(params.allValues(idParamName));
    if (allFlexibleIds.isEmpty()) // No params found.
    {
        NX_LOG(lit("Bad request: No 'cameraId' params specified"), cl_logWARNING);
        return;
    }

    for (const auto& flexibleId: allFlexibleIds)
    {
        if (auto camera = findCameraByFlexibleId(resourcePool, flexibleId))
            *cameras << camera;
        else
            NX_LOG(lit("Camera not found by id %1, ignoring").arg(flexibleId), cl_logWARNING);
    }
}

QnSecurityCamResourcePtr findCameraByFlexibleIds(
    QnResourcePool* resourcePool,
    QString* outNotFoundCameraId,
    const QnRequestParams& params,
    const QStringList& idParamNames)
{
    if (outNotFoundCameraId)
        *outNotFoundCameraId = QString::null;
    QString flexibleIdParamName;
    for (const auto& idParamName: idParamNames)
    {
        if (params.contains(idParamName))
        {
            if (!flexibleIdParamName.isNull()) // More than one param specified.
            {
                NX_LOG(lit(
                    "Bad request: Both %1 and %2 specified to identify a camera")
                    .arg(flexibleIdParamName).arg(idParamName), cl_logWARNING);
                return QnSecurityCamResourcePtr(nullptr);
            }
            flexibleIdParamName = idParamName;
        }
    }
    if (flexibleIdParamName.isNull()) // No params found.
    {
        NX_LOG(lit("Bad request: 'cameraId' param missing"), cl_logWARNING);
        return QnSecurityCamResourcePtr(nullptr);
    }

    QString flexibleId = params.value(flexibleIdParamName);
    NX_ASSERT(!flexibleId.isNull()); //< NOTE: Can be empty if specified empty in the request.

    auto camera = findCameraByFlexibleId(resourcePool, flexibleId);
    if (!camera)
    {
        if (outNotFoundCameraId)
            *outNotFoundCameraId = flexibleId;
    }
    return camera;
}

QnSecurityCamResourcePtr findCameraByFlexibleId(
    QnResourcePool* resourcePool,
    const QString& flexibleId)
{
    QnResourcePtr result;
    const QnUuid uuid = QnUuid::fromStringSafe(flexibleId);
    if (!uuid.isNull())
        result = resourcePool->getResourceById(uuid);
    if (!result)
        result = resourcePool->getNetResourceByPhysicalId(flexibleId);
    if (!result)
        result = resourcePool->getResourceByMacAddress(flexibleId);

    // If the found resource is not a camera, return null.
    return result.dynamicCast<QnSecurityCamResource>();
}

} // namespace camera_id_helper
} // namespace nx
