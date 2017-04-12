#pragma once

#include <core/resource/resource_fwd.h>
#include <utils/common/request_param.h>

namespace nx {

/**
 * Finds cameras by so-called FlexibleId which can be id, physicalId or mac.
 */
class camera_id_helper
{
public:
    /**
     * @param resourcePool Resources pool where search will occur.
     * @param params Expected to contain at least one param from the flexibleIdParamNames list,
     *     otherwise nothing is added to the list and the error is logged.
     * @param request Used for logging.
     */
    static void findAllCamerasByFlexibleIds(
        QnResourcePool* resourcePool,
        QnSecurityCamResourceList* cameras,
        const QnRequestParamList& params,
        const QStringList& flexibleIdParamNames);

    /**
     * @param resourcePool Resources pool where search will occur.
     * @param params Expected to contain exactly one param from the flexibleIdParamNames list,
     *     otherwise result is null and the error is logged.
     * @param outNotFoundCameraId If result is null, this argument (if not null) is set to the
     *     flexibleId in case params are good but the camera is not found by this flexibleId.
     * @return Camera, or null if not found (the error is logged).
     */
    static QnSecurityCamResourcePtr findCameraByFlexibleIds(
        QnResourcePool* resourcePool,
        QString* outNotFoundCameraId,
        const QnRequestParams& params,
        const QStringList& flexibleIdParamNames);

    /**
     * @param resourcePool Resources pool where search will occur.
     * @return Camera, or null if not found.
     */
    static QnSecurityCamResourcePtr findCameraByFlexibleId(
        QnResourcePool* resourcePool,
        const QString& flexibleId);

private:
    camera_id_helper() = delete;
};

} // namespace nx
