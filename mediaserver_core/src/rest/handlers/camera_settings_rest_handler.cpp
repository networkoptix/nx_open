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

namespace {

static const QString kCameraIdParam = lit("cameraId");
static const QString kDeprecatedResIdParam = lit("res_id");

/** Max time (milliseconds) to wait for async operation completion. */
static const qint64 kMaxWaitTimeoutMs = 15000;

} // namespace

struct AwaitedParameters
{
    QnResourcePtr resource;
    QSet<QString> requested;
    QnCameraAdvancedParamValueList result;
};

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
    NX_LOG(this, lm("Received request %1").arg(path), cl_logDEBUG1);

    QString notFoundCameraId = QString::null;
    auto camera = nx::camera_id_helper::findCameraByFlexibleIds(
        owner->resourcePool(),
        &notFoundCameraId,
        params,
        {kCameraIdParam, kDeprecatedResIdParam});
    if (!camera)
    {
        NX_LOG(this, lm("Camera not found"), cl_logWARNING);
        if (notFoundCameraId.isNull())
            return CODE_BAD_REQUEST;
        else
            return CODE_NOT_FOUND;
    }

    if (!owner->resourceAccessManager()->hasPermission(
        owner->accessRights(),
        camera,
        Qn::Permission::ReadPermission))
    {
        NX_LOG(this, lm("No Permissions to execute request %1").arg(path), cl_logWARNING);
        return nx::network::http::StatusCode::forbidden;
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

    // TODO: It would be nice to fill reasonPhrase too.
    if (locParams.empty())
    {
        NX_LOG(this, lm("No valid param names in request %1").arg(path), cl_logWARNING);
        return CODE_BAD_REQUEST;
    }

    Operation operation;
    QString action = extractAction(path);
    if (action == "getCameraParam")
    {
        if (cameraParameters.packet_mode)
            operation = Operation::GetParamsBatch;
        else
            operation = Operation::GetParam;
    }
    else if (action == "setCameraParam")
    {
        if (cameraParameters.packet_mode)
            operation = Operation::SetParamsBatch;
        else
            operation = Operation::SetParam;
    }
    else
    {
        NX_LOG(this, lm("Unknown command %1 in request %2. Ignoring...")
            .arg(action).arg(path), cl_logWARNING);
        return CODE_NOT_FOUND;
    }

    AwaitedParameters awaitedParams;

    awaitedParams.resource = camera;

    connectToResource(camera, operation);

    QnCameraAdvancedParamValueList values;
    for (auto iter = locParams.cbegin(); iter != locParams.cend(); ++iter)
    {
        QnCameraAdvancedParamValue param(iter.key(), iter.value());
        values << param;
        awaitedParams.requested << param.id;
    }
    processOperation(camera, operation, values);

    {
        QnMutexLocker lk(&m_mutex);
        std::set<AwaitedParameters*>::iterator awaitedParamsIter =
            m_awaitedParamsSets.insert(&awaitedParams).first;
        QElapsedTimer asyncOpCompletionTimer;
        asyncOpCompletionTimer.start();
        for (;;)
        {
            const qint64 curClock = asyncOpCompletionTimer.elapsed();

            // Out of time, returning what we have.
            if (curClock >= (int) kMaxWaitTimeoutMs)
                break;

            m_cond.wait(&m_mutex, kMaxWaitTimeoutMs - curClock);

            // Received all parameters.
            if (awaitedParams.requested.empty())
                break;
        }
        m_awaitedParamsSets.erase(awaitedParamsIter);
    }

    disconnectFromResource(camera, operation);

    // Serializing answer.
    QnCameraAdvancedParamValueList reply = awaitedParams.result;
    result.setReply(reply);

    NX_LOG(this, lm("Request %1 processed successfully for camera %2")
        .arg(path).arg(camera->getId().toString()),
        cl_logDEBUG1);
    return CODE_OK;
}

void QnCameraSettingsRestHandler::connectToResource(
    const QnResourcePtr& resource, Operation operation)
{
    switch (operation)
    {
        case Operation::GetParam:
            connect(resource, &QnResource::asyncParamGetDone,
                this, &QnCameraSettingsRestHandler::asyncParamGetComplete, Qt::DirectConnection);
            break;
        case Operation::GetParamsBatch:
            connect(resource, &QnResource::asyncParamsGetDone,
                this, &QnCameraSettingsRestHandler::asyncParamsGetComplete, Qt::DirectConnection);
            break;
        case Operation::SetParam:
            connect(resource, &QnResource::asyncParamSetDone,
                this, &QnCameraSettingsRestHandler::asyncParamSetComplete, Qt::DirectConnection);
            break;
        case Operation::SetParamsBatch:
            connect(resource, &QnResource::asyncParamsSetDone,
                this, &QnCameraSettingsRestHandler::asyncParamsSetComplete, Qt::DirectConnection);
            break;
    }
}

void QnCameraSettingsRestHandler::disconnectFromResource(
    const QnResourcePtr& resource, Operation operation)
{
    switch (operation)
    {
        case Operation::GetParam:
            disconnect(resource, &QnResource::asyncParamGetDone,
                this, &QnCameraSettingsRestHandler::asyncParamGetComplete);
            break;
        case Operation::GetParamsBatch:
            disconnect(resource, &QnResource::asyncParamsGetDone,
                this, &QnCameraSettingsRestHandler::asyncParamsGetComplete);
            break;
        case Operation::SetParam:
            disconnect(resource, &QnResource::asyncParamSetDone,
                this, &QnCameraSettingsRestHandler::asyncParamSetComplete);
            break;
        case Operation::SetParamsBatch:
            disconnect(resource, &QnResource::asyncParamsSetDone,
                this, &QnCameraSettingsRestHandler::asyncParamsSetComplete);
            break;
    }
}

void QnCameraSettingsRestHandler::processOperation(
    const QnResourcePtr& resource,
    Operation operation,
    const QnCameraAdvancedParamValueList& values)
{
    switch (operation)
    {
        case Operation::GetParam:
            for (const QnCameraAdvancedParamValue value: values)
                resource->getParamPhysicalAsync(value.id);
            break;
        case Operation::GetParamsBatch:
        {
            QSet<QString> ids;
            for (const QnCameraAdvancedParamValue value: values)
                ids.insert(value.id);
            resource->getParamsPhysicalAsync(ids);
            break;
        }
        case Operation::SetParam:
            for (const QnCameraAdvancedParamValue value: values)
                resource->setParamPhysicalAsync(value.id, value.value);
            break;
        case Operation::SetParamsBatch:
            resource->setParamsPhysicalAsync(values);
            break;
    }
}

void QnCameraSettingsRestHandler::asyncParamGetComplete(
    const QnResourcePtr& resource, const QString& id, const QString& value, bool success)
{
    QnMutexLocker lk(&m_mutex);

    NX_LOG(lit("QnCameraSettingsHandler::asyncParamGetComplete. paramName %1, paramValue %2")
        .arg(id).arg(value), cl_logDEBUG1);
    for (std::set<AwaitedParameters*>::const_iterator it =
        m_awaitedParamsSets.begin();
        it != m_awaitedParamsSets.end();
        ++it)
    {
        auto awaitedParams = *it;
        if (!awaitedParams || awaitedParams->resource != resource)
            continue;

        if (!awaitedParams->requested.contains(id))
            continue;

        if (success)
            awaitedParams->result << QnCameraAdvancedParamValue(id, value);

        awaitedParams->requested.remove(id);
    }

    m_cond.wakeAll();
}

void QnCameraSettingsRestHandler::asyncParamSetComplete(
    const QnResourcePtr& resource, const QString& id, const QString& value, bool success)
{
    // Processing is identical to the previous method.
    asyncParamGetComplete(resource, id, value, success);
}

void QnCameraSettingsRestHandler::asyncParamsGetComplete(
    const QnResourcePtr& resource, const QnCameraAdvancedParamValueList& values)
{
    QnMutexLocker lk(&m_mutex);

    for (std::set<AwaitedParameters*>::const_iterator
        it = m_awaitedParamsSets.begin();
        it != m_awaitedParamsSets.end();
        ++it)
    {
        auto awaitedParams = *it;
        if (!awaitedParams || awaitedParams->resource != resource)
            continue;

        for (const QnCameraAdvancedParamValue& value: values)
        {
            if (!awaitedParams->requested.contains(value.id))
                continue;
            awaitedParams->result << value;
        }
        awaitedParams->requested.clear();
    }
    m_cond.wakeAll();
}

void QnCameraSettingsRestHandler::asyncParamsSetComplete(
    const QnResourcePtr& resource, const QnCameraAdvancedParamValueList& values)
{
    // Processing is identical to the previous method.
    asyncParamsGetComplete(resource, values);
}
