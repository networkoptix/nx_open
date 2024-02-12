// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "camera_id_helper.h"

#include <core/resource_management/resource_pool.h>
#include <core/resource/camera_resource.h>
#include <nx/utils/log/log.h>
#include <nx/network/rest/params.h>

namespace nx {
namespace camera_id_helper {

void findAllCamerasByFlexibleIds(
    QnResourcePool* resourcePool,
    QnVirtualCameraResourceList* cameras,
    const nx::network::rest::Params& params,
    const QStringList& flexibleIdParamNames)
{
    QStringList allFlexibleIds;
    for (const auto& idParamName: flexibleIdParamNames)
        allFlexibleIds.append(params.values(idParamName));

    if (allFlexibleIds.isEmpty()) // No params found.
    {
        NX_VERBOSE(NX_SCOPE_TAG, "No 'cameraId' params specified");
        return;
    }

    for (const auto& flexibleId: allFlexibleIds)
    {
        if (auto camera = findCameraByFlexibleId(resourcePool, flexibleId))
            *cameras << camera;
        else
            NX_WARNING(NX_SCOPE_TAG, "Camera not found by id %1, ignoring", flexibleId);
    }
}

QnVirtualCameraResourcePtr findCameraByFlexibleIds(
    QnResourcePool* resourcePool,
    QString* outNotFoundCameraId,
    const nx::network::rest::Params& params,
    const QStringList& idParamNames)
{
    if (outNotFoundCameraId)
        *outNotFoundCameraId = QString();
    QString flexibleIdParamName;
    for (const auto& idParamName: idParamNames)
    {
        if (params.contains(idParamName))
        {
            if (!flexibleIdParamName.isNull()) // More than one param specified.
            {
                NX_WARNING(NX_SCOPE_TAG,
                    "Bad request: Both %1 and %2 specified to identify a camera",
                    flexibleIdParamName, idParamName);
                return QnVirtualCameraResourcePtr(nullptr);
            }
            flexibleIdParamName = idParamName;
        }
    }
    if (flexibleIdParamName.isNull()) // No params found.
    {
        NX_WARNING(NX_SCOPE_TAG, "Bad request: 'cameraId' param missing");
        return QnVirtualCameraResourcePtr(nullptr);
    }

    QString flexibleId = params.value(flexibleIdParamName);
    if (flexibleId.isNull())
    {
        NX_DEBUG(NX_SCOPE_TAG, "Bad request: %1 is null", flexibleIdParamName);
        return QnVirtualCameraResourcePtr(nullptr);
    }

    auto camera = findCameraByFlexibleId(resourcePool, flexibleId);
    if (!camera)
    {
        if (outNotFoundCameraId)
            *outNotFoundCameraId = flexibleId;
    }
    return camera;
}

nx::Uuid flexibleIdToId(
    const QnResourcePool* resourcePool,
    const QString& flexibleId)
{
    if (!resourcePool)
        return nx::Uuid();
    auto camera = findCameraByFlexibleId(resourcePool, flexibleId);
    return camera ? camera->getId() : nx::Uuid();
}

QnVirtualCameraResourcePtr findCameraByFlexibleId(
    const QnResourcePool* resourcePool,
    const QString& flexibleId)
{
    if (const nx::Uuid uuid = nx::Uuid::fromStringSafe(flexibleId); !uuid.isNull())
    {
        if (auto camera = resourcePool->getResourceById<QnVirtualCameraResource>(uuid))
            return camera;
    }

    if (auto camera = resourcePool->getResourceByPhysicalId<QnVirtualCameraResource>(flexibleId))
        return camera;

    if (auto camera = resourcePool->getResourceByMacAddress(flexibleId)
        .dynamicCast<QnVirtualCameraResource>())
    {
        return camera;
    }

    if (const int logicalId = flexibleId.toInt(); logicalId > 0)
    {
        auto cameraList = resourcePool->getResourcesByLogicalId(logicalId)
            .filtered<QnVirtualCameraResource>();
        if (!cameraList.isEmpty())
            return cameraList.front();
    }

    return QnVirtualCameraResourcePtr();
}

} // namespace camera_id_helper
} // namespace nx
