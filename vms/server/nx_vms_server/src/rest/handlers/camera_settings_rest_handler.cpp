#include "camera_settings_rest_handler.h"

#include <algorithm>

#include <QString>
#include <QMap>

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
#include <core/resource/resource_command.h>
#include <media_server/media_server_module.h>
#include <core/resource/resource_command_processor.h>

static const QString kCameraIdParam = "cameraId";
static const QString kDeprecatedResIdParam = "res_id";
static const std::chrono::seconds kMaxWaitTimeout(20);

using StatusCode = nx::network::http::StatusCode::Value;
using PostBody = QnCameraSettingsRestHandlerPostBody;

struct QnCameraSettingsRestHandlerPostBody
{
    QString cameraId;
    QMap<QString, QString> paramValues;
};
#define QnCameraSettingsRestHandlerPostBody_Fields (cameraId)(paramValues)

QN_FUSION_ADAPT_STRUCT_FUNCTIONS(QnCameraSettingsRestHandlerPostBody, (json),
    QnCameraSettingsRestHandlerPostBody_Fields);

namespace {

class GetAdvancedParametersCommand: public QnResourceCommand
{
public:
    GetAdvancedParametersCommand(
        const QnResourcePtr& resource,
        const QSet<QString>& ids,
        std::function<void(const QnCameraAdvancedParamValueMap&)> handler)
        :
        QnResourceCommand(resource),
        m_ids(ids),
        m_handler(std::move(handler))
    {
    }

    bool execute() override
    {
        const auto camera = m_resource.dynamicCast<nx::vms::server::resource::Camera>();
        NX_CRITICAL(camera);

        QnCameraAdvancedParamValueMap values;
        if (isConnectedToTheResource())
            values = camera->getAdvancedParameters(m_ids);

        m_handler(values);
        return true;
    }

    void beforeDisconnectFromResource() override
    {
    }

private:
    QSet<QString> m_ids;
    std::function<void(const QnCameraAdvancedParamValueMap&)> m_handler;
};

class SetAdvancedParametersCommand: public QnResourceCommand
{
public:
    SetAdvancedParametersCommand(
        const QnResourcePtr& resource,
        const QnCameraAdvancedParamValueMap& values,
        std::function<void(const QSet<QString>&)> handler)
        :
        QnResourceCommand(resource),
        m_values(values),
        m_handler(std::move(handler))
    {
    }

    bool execute() override
    {
        const auto camera = m_resource.dynamicCast<nx::vms::server::resource::Camera>();
        NX_CRITICAL(camera);

        QSet<QString> ids;
        if (isConnectedToTheResource())
            ids = camera->setAdvancedParameters(m_values);

        m_handler(ids);
        return true;
    }

    void beforeDisconnectFromResource() override
    {
    }

private:
    QnCameraAdvancedParamValueMap m_values;
    std::function<void(const QSet<QString>&)> m_handler;
};

} // namespace

QnCameraSettingsRestHandler::QnCameraSettingsRestHandler(QnResourceCommandProcessor* commandProcessor):
    base_type(),
    m_paramsReader(new QnCachingCameraAdvancedParamsReader()),
    m_commandProcessor(commandProcessor)
{
}

QnCameraSettingsRestHandler::~QnCameraSettingsRestHandler()
{
}

StatusCode QnCameraSettingsRestHandler::obtainCameraFromRequestParams(
    const QnRequestParams& params,
    QnJsonRestResult& result,
    const QnRestConnectionProcessor* owner,
    nx::vms::server::resource::CameraPtr* outCamera) const
{
    QString notFoundCameraId = QString::null;

    *outCamera = nx::camera_id_helper::findCameraByFlexibleIds(
        owner->resourcePool(),
        &notFoundCameraId,
        params,
        {kCameraIdParam, kDeprecatedResIdParam}
    ).dynamicCast<nx::vms::server::resource::Camera>();

    if (!*outCamera)
    {
        NX_WARNING(this, "Camera not found");
        if (notFoundCameraId.isNull())
        {
            result.setError(QnRestResult::MissingParameter, "Camera is not specified");
            return StatusCode::unprocessableEntity;
        }
        else
        {
            result.setError(QnRestResult::InvalidParameter, lm("No camera %1").arg(notFoundCameraId));
            return StatusCode::unprocessableEntity;
        }
    }

    return StatusCode::ok;
}

StatusCode QnCameraSettingsRestHandler::obtainCameraFromPostBody(
    const PostBody& postBody,
    QnJsonRestResult& result,
    const QnRestConnectionProcessor* owner,
    nx::vms::server::resource::CameraPtr* outCamera) const
{
    *outCamera = nx::camera_id_helper::findCameraByFlexibleId(
        owner->resourcePool(),
        postBody.cameraId
    ).dynamicCast<nx::vms::server::resource::Camera>();

    if (!*outCamera)
    {
        NX_WARNING(this, "Camera not found by cameraId %1", postBody.cameraId);
        result.setError(QnRestResult::InvalidParameter, lm("No camera %1").arg(postBody.cameraId));
        return StatusCode::unprocessableEntity;
    }

    return StatusCode::ok;
}

StatusCode QnCameraSettingsRestHandler::obtainCameraParamValuesFromRequestParams(
    const nx::vms::server::resource::CameraPtr& camera,
    const QnRequestParams& params,
    QnJsonRestResult& result,
    QnCameraAdvancedParamValueMap* outValues) const
{
    // Remove params that are not camera params.
    QnRequestParams cameraParams = params;
    cameraParams.remove(Qn::SERVER_GUID_HEADER_NAME);
    cameraParams.remove(kCameraIdParam);
    cameraParams.remove(kDeprecatedResIdParam);

    // Filter allowed parameters.
    const auto allowedParams = m_paramsReader->params(camera).allParameterIds();
    for (auto it = cameraParams.begin(); it != cameraParams.end(); ++it)
    {
        if (allowedParams.contains(it.key()))
            outValues->insert(it.key(), it.value());
    }

    if (outValues->empty())
    {
        result.setError(QnRestResult::MissingParameter,
            "No valid camera parameters in the request");
        return StatusCode::unprocessableEntity;
    }

    return StatusCode::ok;
}

StatusCode QnCameraSettingsRestHandler::obtainCameraParamValuesFromPostBody(
    const nx::vms::server::resource::CameraPtr& camera,
    const PostBody& postBody,
    QnJsonRestResult& result,
    QnCameraAdvancedParamValueMap* outValues) const
{
    // Filter allowed parameters.
    const auto allowedParams = m_paramsReader->params(camera).allParameterIds();
    for (auto it = postBody.paramValues.begin(); it != postBody.paramValues.end(); ++it)
    {
        if (allowedParams.contains(it.key()))
            outValues->insert(it.key(), it.value());
    }

    if (outValues->empty())
    {
        result.setError(QnRestResult::MissingParameter,
            "No valid camera parameters in the request");
        return StatusCode::unprocessableEntity;
    }

    return StatusCode::ok;
}

int QnCameraSettingsRestHandler::executeGet(
    const QString& path,
    const QnRequestParams& params,
    QnJsonRestResult& result,
    const QnRestConnectionProcessor* owner)
{
    NX_DEBUG(this, "Received request GET %1", path);

    auto statusCode = StatusCode::undefined;

    nx::vms::server::resource::CameraPtr camera;
    statusCode = obtainCameraFromRequestParams(params, result, owner, &camera);
    if (statusCode != StatusCode::ok)
        return statusCode;

    QnCameraAdvancedParamValueMap values;
    statusCode = obtainCameraParamValuesFromRequestParams(camera, params, result, &values);
    if (statusCode != StatusCode::ok)
        return statusCode;

    const auto action = extractAction(path);

    QnCameraAdvancedParamValueMap outParameterMap;
    if (action == "getCameraParam")
    {
        statusCode = handleGetParamsRequest(
            owner,
            camera,
            values.ids(),
            &outParameterMap);
    }
    else if (action == "setCameraParam")
    {
        statusCode = handleSetParamsRequest(owner, camera, values, &outParameterMap);
    }
    else
    {
        result.setError(QnRestResult::InvalidParameter, lm("Unknown command: %1").arg(action));
        return StatusCode::unprocessableEntity;
    }

    if (statusCode != StatusCode::ok)
    {
        result.setError(QnRestResult::CantProcessRequest,
            nx::network::http::StatusCode::toString(statusCode));
        return statusCode;
    }

    NX_DEBUG(this, "Request GET %1 processed successfully for camera %2: %3",
        path, camera->getId(), containerString(outParameterMap));

    result.setReply(outParameterMap.toValueList());
    return StatusCode::ok;
}

int QnCameraSettingsRestHandler::executePost(
    const QString& path,
    const QnRequestParams& /*params*/,
    const QByteArray& body,
    QnJsonRestResult& result,
    const QnRestConnectionProcessor* owner)
{
    NX_DEBUG(this, "Received request POST %1", path);

    bool success = false;
    const auto postBody = QJson::deserialized<PostBody>(body, PostBody(), &success);
    if (!success)
    {
        result.setError(QnJsonRestResult::InvalidParameter, "Invalid JSON object provided");
        return nx::network::http::StatusCode::ok;
    }

    auto statusCode = StatusCode::undefined;

    nx::vms::server::resource::CameraPtr camera;
    statusCode = obtainCameraFromPostBody(postBody, result, owner, &camera);
    if (statusCode != StatusCode::ok)
        return statusCode;

    QnCameraAdvancedParamValueMap values;
    statusCode = obtainCameraParamValuesFromPostBody(camera, postBody, result, &values);
    if (statusCode != StatusCode::ok)
        return statusCode;

    QnCameraAdvancedParamValueMap outParameterMap;
    statusCode = handleSetParamsRequest(owner, camera, values, &outParameterMap);
    if (statusCode != StatusCode::ok)
    {
        result.setError(QnRestResult::CantProcessRequest,
            nx::network::http::StatusCode::toString(statusCode));
        return statusCode;
    }

    NX_DEBUG(this, "Request POST %1 processed successfully for camera %2: %3",
        path, camera->getId(), containerString(outParameterMap));

    result.setReply(outParameterMap.toValueList());
    return StatusCode::ok;
}

StatusCode QnCameraSettingsRestHandler::handleGetParamsRequest(
    const QnRestConnectionProcessor* owner,
    const QnVirtualCameraResourcePtr& camera,
    const QSet<QString>& requestedParameterIds,
    QnCameraAdvancedParamValueMap* outParameterMap)
{
    const bool hasPermissions = owner->resourceAccessManager()->hasPermission(
        owner->accessRights(),
        camera,
        Qn::Permission::ReadPermission);

    if (!hasPermissions)
        return nx::network::http::StatusCode::forbidden;

    nx::utils::AsyncOperationGuard operationGuard;
    nx::utils::promise<void> operationPromise;

    m_commandProcessor->putData(
        std::make_shared<GetAdvancedParametersCommand>(
            camera,
            requestedParameterIds,
            [&, guard = operationGuard.sharedGuard()](
                const QnCameraAdvancedParamValueMap& resultParams)
            {
                if (const auto lock = guard->lock())
                {
                    *outParameterMap = resultParams;
                    operationPromise.set_value();
                }
            }));

    if (operationPromise.get_future().wait_for(kMaxWaitTimeout) != std::future_status::ready)
    {
        operationGuard.reset();
        return StatusCode::requestTimeOut;
    }

    return StatusCode::ok;
}

StatusCode QnCameraSettingsRestHandler::handleSetParamsRequest(
    const QnRestConnectionProcessor* owner,
    const QnVirtualCameraResourcePtr& camera,
    const QnCameraAdvancedParamValueMap& parametersToSet,
    QnCameraAdvancedParamValueMap* outParameterMap)
{
    const auto accessManager = owner->resourceAccessManager();
    const auto accessRights = owner->accessRights();
    const auto hasPermissions =
        accessManager->hasGlobalPermission(accessRights, GlobalPermission::editCameras)
        && accessManager->hasPermission(accessRights, camera, Qn::Permission::WritePermission);

    if (!hasPermissions)
        return nx::network::http::StatusCode::forbidden;

    nx::utils::AsyncOperationGuard operationGuard;
    nx::utils::promise<void> operationPromise;

    m_commandProcessor->putData(
        std::make_shared<SetAdvancedParametersCommand>(
            camera,
            parametersToSet,
            [&, guard = operationGuard.sharedGuard()](const QSet<QString>& resultIds)
            {
                if (const auto lock = guard->lock())
                {
                    for (const auto& id: resultIds)
                    {
                        if (parametersToSet.contains(id))
                            outParameterMap->insert(id, parametersToSet[id]);
                    }

                    operationPromise.set_value();
                }
            }));

    if (operationPromise.get_future().wait_for(kMaxWaitTimeout) != std::future_status::ready)
    {
        operationGuard.reset();
        return StatusCode::requestTimeOut;
    }

    const bool needToReloadAllParameters = camera->resourceData().value<bool>(
        ResourceDataKey::kNeedToReloadAllAdvancedParametersAfterApply, false);

    if (needToReloadAllParameters)
    {
        const auto allParameterIds = QnCameraAdvancedParamsReader::paramsFromResource(camera)
            .allParameterIds();

        outParameterMap->clear();
        return handleGetParamsRequest(owner, camera, allParameterIds, outParameterMap);
    }

    return StatusCode::ok;
}
