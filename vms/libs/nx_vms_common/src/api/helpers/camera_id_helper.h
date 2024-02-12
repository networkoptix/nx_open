// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <core/resource/resource_fwd.h>

namespace nx::network::rest { class Params; }

namespace nx {

/**
 * Finds cameras by so-called FlexibleId which can be id, physicalId, or mac.
 */
namespace camera_id_helper {

/**
 * @param resourcePool Resource pool where the search will occur.
 * @param params Expected to contain at least one param from the flexibleIdParamNames list,
 *     otherwise nothing is added to the list and the error is logged.
 * @param request Used for logging.
 */
void NX_VMS_COMMON_API findAllCamerasByFlexibleIds(
    QnResourcePool* resourcePool,
    QnVirtualCameraResourceList* cameras,
    const nx::network::rest::Params& params,
    const QStringList& flexibleIdParamNames);

/**
 * @param resourcePool Resource pool where the search will occur.
 * @param params Expected to contain exactly one param from the flexibleIdParamNames list,
 *     otherwise result is null and the error is logged.
 * @param outNotFoundCameraId If result is null, this argument (if not null) is set to the
 *     flexibleId in case params are good but the camera is not found by this flexibleId.
 * @return Camera, or null if not found (the error is logged).
 */
NX_VMS_COMMON_API QnVirtualCameraResourcePtr findCameraByFlexibleIds(
    QnResourcePool* resourcePool,
    QString* outNotFoundCameraId,
    const nx::network::rest::Params& params,
    const QStringList& flexibleIdParamNames);

/**
 * @param resourcePool Resources pool where the search will occur.
 * @return Camera, or null if not found.
 */
NX_VMS_COMMON_API QnVirtualCameraResourcePtr findCameraByFlexibleId(
    const QnResourcePool* resourcePool,
    const QString& flexibleId);

template <class IdList>
QnVirtualCameraResourceList findCamerasByFlexibleId(
    const QnResourcePool* resourcePool,
    const IdList& flexibleIds)
{
    QnVirtualCameraResourceSet result;
    for (const QString& id: flexibleIds)
    {
        if (const auto camera = findCameraByFlexibleId(resourcePool, id))
            result.insert(camera);
    }
    return result.values();
}

/**
 * @param resourcePool Resources pool where the search will occur.
 * @return Camera id, or null if not found.
 */
NX_VMS_COMMON_API nx::Uuid flexibleIdToId(
    const QnResourcePool* resourcePool,
    const QString& flexibleId);

} // namespace camera_id_helper

} // namespace nx
