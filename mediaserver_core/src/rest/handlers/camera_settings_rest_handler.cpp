#include "camera_settings_rest_handler.h"

#include <algorithm>

#include <core/resource_management/resource_pool.h>
#include <core/resource/resource.h>
#include <core/resource/security_cam_resource.h>
#include <core/resource/camera_advanced_param.h>
#include <core/resource_access/resource_access_manager.h>

#include <nx/utils/log/log.h>
#include <nx/network/http/http_types.h>
#include <nx/fusion/model_functions.h>
#include <network/tcp_connection_priv.h>
#include <rest/server/rest_connection_processor.h>

#include <utils/xml/camera_advanced_param_reader.h>
#include <nx/network/http/custom_headers.h>
#include <api/helpers/camera_id_helper.h>
#include <nx/utils/std/future.h>
#include <nx/utils/async_operation_guard.h>

static const QString kCameraIdParam = lit("cameraId");
static const QString kDeprecatedResIdParam = lit("res_id");
static const std::chrono::seconds kMaxWaitTimeout(15);

using StatusCode = nx::network::http::StatusCode::Value;

QnCameraSettingsRestHandler::QnCameraSettingsRestHandler():
    base_type(),
    m_paramsReader(new QnCachingCameraAdvancedParamsReader())
{
}

QnCameraSettingsRestHandler::~QnCameraSettingsRestHandler()
{
}

int QnCameraSettingsRestHandler::executeGet(
    const QString& path, const QnRequestParams& params, QnJsonRestResult& result,
    const QnRestConnectionProcessor* owner)
{
    NX_DEBUG(this, lm("Received request %1").arg(path));
    QString notFoundCameraId = QString::null;
    auto camera = nx::camera_id_helper::findCameraByFlexibleIds(
        owner->resourcePool(),
        &notFoundCameraId,
        params,
        {kCameraIdParam, kDeprecatedResIdParam})
            .dynamicCast<nx::mediaserver::resource::Camera>();
    if (!camera)
    {
        NX_LOG(this, lm("Camera not found"), cl_logWARNING);
        if (notFoundCameraId.isNull())
        {
            result.setError(QnRestResult::MissingParameter, lit("Camera is not specified"));
            return StatusCode::ok;
        }
        else
        {
            result.setError(QnRestResult::InvalidParameter, lm("No camera %1").arg(notFoundCameraId));
            return StatusCode::ok;
        }
    }

    if (!owner->resourceAccessManager()->hasPermission(
        owner->accessRights(),
        camera,
        Qn::Permission::ReadPermission))
    {
        return StatusCode::forbidden;
    }

    // Clean params that are not keys.
    QnRequestParams locParams = params;
    locParams.remove(Qn::SERVER_GUID_HEADER_NAME);
    locParams.remove(kCameraIdParam);
    locParams.remove(kDeprecatedResIdParam);

    // Filter allowed parameters.
    QnCameraAdvancedParams cameraParameters = m_paramsReader->params(camera);
    auto allowedParams = cameraParameters.allParameterIds();

    for (auto iter = locParams.begin(); iter != locParams.end();)
    {
        if (allowedParams.contains(iter.key()))
            ++iter;
        else
            iter = locParams.erase(iter);
    }

    if (locParams.empty())
    {
        result.setError(QnRestResult::MissingParameter, lit("No valid camera parameters in request"));
        return StatusCode::ok;
    }

    QnCameraAdvancedParamValueMap values;
    for (auto iter = locParams.cbegin(); iter != locParams.cend(); ++iter)
        values.insert(iter.key(), iter.value());

    nx::utils::AsyncOperationGuard operationGuard;
    nx::utils::promise<void> operationPromise;

    const auto action = extractAction(path);
    if (action == "getCameraParam")
    {
        camera->getAdvancedParametersAsync(
            values.ids(),
            [&, guard = operationGuard.sharedGuard()](const QnCameraAdvancedParamValueMap& result)
            {
                if (const auto lock = guard->lock())
                {
                    values = result;
                    operationPromise.set_value();
                }
            });
    }
    else if (action == "setCameraParam")
    {
        camera->setAdvancedParametersAsync(
            values,
            [&, guard = operationGuard.sharedGuard()](const QSet<QString>& result)
            {
                if (const auto lock = guard->lock())
                {
                    for (auto it = values.begin(); it != values.end(); )
                    {
                        if (result.contains(it.key()))
                            ++it;
                        else
                            it = values.erase(it);
                    }

                    operationPromise.set_value();
                }
            });
    }
    else
    {
        result.setError(QnRestResult::InvalidParameter, lm("Unknown command: %1").arg(action));
        return StatusCode::ok;
    }

    if (operationPromise.get_future().wait_for(kMaxWaitTimeout) != std::future_status::ready)
    {
        operationGuard.reset();
        result.setError(QnRestResult::CantProcessRequest, lit("Timed out"));
        return StatusCode::ok;
    }

    NX_DEBUG(this, lm("Request %1 processed successfully for camera %2: %3")
        .args(path, camera->getId(), containerString(values)));

    result.setReply(values.toValueList());
    return StatusCode::ok;
}
