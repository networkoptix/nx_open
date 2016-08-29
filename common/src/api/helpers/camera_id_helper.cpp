#include "camera_id_helper.h"

#include <core/resource_management/resource_pool.h>
#include <core/resource/security_cam_resource.h>
#include <nx/utils/log/log.h>

namespace nx {

void camera_id_helper::findAllCamerasByFlexibleIds(
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
        if (auto camera = findCameraByFlexibleId(flexibleId))
            *cameras << camera;
        else
            NX_LOG(lit("Camera not found by id %1, ignoring").arg(flexibleId), cl_logWARNING);
    }
}

QnSecurityCamResourcePtr camera_id_helper::findCameraByFlexibleIds(
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

    QString flexibleId = params[flexibleIdParamName];
    NX_ASSERT(!flexibleId.isNull()); //< NOTE: Can be empty if specified empty in the request.

    auto camera = findCameraByFlexibleId(flexibleId);
    if (!camera)
    {
        if (outNotFoundCameraId)
            *outNotFoundCameraId = flexibleId;
    }
    return camera;
}

QnSecurityCamResourcePtr camera_id_helper::findCameraByFlexibleId(const QString& flexibleId)
{
    QnResourcePtr result;
    const QnUuid uuid = QnUuid::fromStringSafe(flexibleId);
    if (!uuid.isNull())
        result = qnResPool->getResourceById(uuid);
    if (!result)
        result = qnResPool->getNetResourceByPhysicalId(flexibleId);
    if (!result)
        result = qnResPool->getResourceByMacAddress(flexibleId);

    // If the found resource is not a camera, return null.
    return result.dynamicCast<QnSecurityCamResource>();
}

} // namespace nx
