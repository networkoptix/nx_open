#include "ptz_rest_handler.h"

#include <QtConcurrent/QtConcurrent>

#include <nx/fusion/serialization/json_functions.h>
#include <nx/fusion/serialization/lexical.h>
#include <network/tcp_connection_priv.h>

#include <core/resource_management/resource_pool.h>
#include <core/resource/camera_resource.h>
#include <core/ptz/abstract_ptz_controller.h>
#include <core/ptz/ptz_data.h>
#include <core/ptz/ptz_controller_pool.h>
#include <nx/network/http/http_types.h>
#include <rest/server/rest_connection_processor.h>
#include <core/resource_access/resource_access_manager.h>
#include <core/resource/user_resource.h>
#include <api/helpers/camera_id_helper.h>
#include <nx/utils/log/log.h>
#include <common/common_module.h>
#include <rest/server/rest_connection_processor.h>
#include <network/tcp_listener.h>
#include <nx/utils/log/log_main.h>

namespace {

static const QString kCameraIdParam = lit("cameraId");
static const QString kDeprecatedResourceIdParam = lit("resourceId");
static const QStringList kCameraIdParams{kCameraIdParam, kDeprecatedResourceIdParam};

static const int OLD_SEQUENCE_THRESHOLD = 1000 * 60 * 5;

bool checkUserAccess(
    const Qn::UserAccessData& accessRights,
    const QnSecurityCamResourcePtr& camera,
    Qn::PtzCommand command)
{
    const auto& accessManager = camera->commonModule()->resourceAccessManager();
    switch (command)
    {
        case Qn::ContinuousMovePtzCommand:
        case Qn::ContinuousFocusPtzCommand:
        case Qn::AbsoluteDeviceMovePtzCommand:
        case Qn::AbsoluteLogicalMovePtzCommand:
        case Qn::ViewportMovePtzCommand:
        case Qn::ActivateTourPtzCommand:
        case Qn::UpdateHomeObjectPtzCommand:
        case Qn::RunAuxilaryCommandPtzCommand:
        case Qn::ActivatePresetPtzCommand:
        {
            if (!accessManager->hasPermission(accessRights, camera, Qn::WritePtzPermission))
                return false;
            return true;
        }

        case Qn::CreateTourPtzCommand:
        case Qn::RemoveTourPtzCommand:
        case Qn::CreatePresetPtzCommand:
        case Qn::UpdatePresetPtzCommand:
        case Qn::RemovePresetPtzCommand:
        {
            if (!accessManager->hasPermission(accessRights, camera, Qn::SavePermission) ||
                !accessManager->hasPermission(accessRights, camera, Qn::WritePtzPermission))
            {
                return false;
            }
            return true;
        }

        case Qn::GetDevicePositionPtzCommand:
        case Qn::GetLogicalPositionPtzCommand:
        case Qn::GetPresetsPtzCommand:
        case Qn::GetToursPtzCommand:
        case Qn::GetActiveObjectPtzCommand:
        case Qn::GetHomeObjectPtzCommand:
        case Qn::GetAuxilaryTraitsPtzCommand:
        case Qn::GetDataPtzCommand:
        default:
            return true;
    }

    return true;
}

} // namespace

QMap<QString, QnPtzRestHandler::AsyncExecInfo> QnPtzRestHandler::m_workers;
QnMutex QnPtzRestHandler::m_asyncExecMutex;

void QnPtzRestHandler::cleanupOldSequence()
{
    QMap<QString, SequenceInfo>::iterator itr = m_sequencedRequests.begin();
    while (itr != m_sequencedRequests.end())
    {
        SequenceInfo& info = itr.value();
        if (info.m_timer.elapsed() > OLD_SEQUENCE_THRESHOLD)
            itr = m_sequencedRequests.erase(itr);
        else
            ++itr;
    }
}

bool QnPtzRestHandler::checkSequence(const QString& id, int sequence)
{
    QnMutexLocker lock(&m_sequenceMutex);
    cleanupOldSequence();
    if (id.isEmpty() || sequence == -1)
        return true; // do not check if empty

    if (m_sequencedRequests[id].sequence > sequence)
    {
        NX_VERBOSE(
            this,
            lit("Check sequence failed. expected sequence >= %1. got sequence %2")
            .arg(m_sequencedRequests[id].sequence).arg(sequence));
        return false;
    }

    m_sequencedRequests[id] = SequenceInfo(sequence);
    return true;
}

void QnPtzRestHandler::asyncExecutor(const QString& sequence, AsyncFunc function)
{
    function();

    m_asyncExecMutex.lock();

    while (AsyncFunc nextFunction = m_workers[sequence].nextCommand)
    {
        NX_VERBOSE("QnPtzRestHandler", lm("Before execute PTZ command sync. Sequence %1").arg(sequence));
        m_workers[sequence].nextCommand = AsyncFunc();
        NX_VERBOSE("QnPtzRestHandler", lm("After execute PTZ command sync. Sequence %1").arg(sequence));
        m_asyncExecMutex.unlock();
        nextFunction();
        m_asyncExecMutex.lock();
    }

    m_workers.remove(sequence);
    m_asyncExecMutex.unlock();
}

int QnPtzRestHandler::execCommandAsync(const QString& sequence, AsyncFunc function)
{
    QnMutexLocker lock(&m_asyncExecMutex);

    if (m_workers[sequence].inProgress)
    {
        NX_VERBOSE("QnPtzRestHandler", lm("Postpone executing async PTZ command because of current worker. Sequence %1").arg(sequence));
        m_workers[sequence].nextCommand = function;
    }
    else
    {
        m_workers[sequence].inProgress = true;
        NX_VERBOSE("QnPtzRestHandler", lm("Start executing async PTZ command. Sequence %1").arg(sequence));
        QtConcurrent::run(std::bind(&QnPtzRestHandler::asyncExecutor, sequence, function));
    }
    return CODE_OK;
}

QStringList QnPtzRestHandler::cameraIdUrlParams() const
{
    return kCameraIdParams;
}

int QnPtzRestHandler::executePost(
    const QString& path,
    const QnRequestParams& params,
    const QByteArray& body,
    QnJsonRestResult& result,
    const QnRestConnectionProcessor* processor)
{
    QString paramStr;
    for (auto itr = params.begin(); itr != params.end(); ++itr)
        paramStr += lit("%1=%2;").arg(itr.key()).arg(itr.value());

    NX_LOG(lit("QnPtzRestHandler: received request %1 %2").arg(path).arg(paramStr), cl_logDEBUG1);

    QString sequenceId;
    int sequenceNumber = -1;
    Qn::PtzCommand command;

    if (!requireParameter(params, lit("command"), result, &command) ||
        !requireParameter(params, lit("sequenceId"), result, &sequenceId, true) ||
        !requireParameter(params, lit("sequenceNumber"), result, &sequenceNumber, true))
    {
        return CODE_INVALID_PARAMETER;
    }

    QString hash = QString(lit("%1-%2"))
        .arg(params.value(kDeprecatedResourceIdParam)).arg(params.value("sequenceId"));

    QString notFoundCameraId = QString::null;
    QnSecurityCamResourcePtr camera = nx::camera_id_helper::findCameraByFlexibleIds(
        processor->owner()->resourcePool(), &notFoundCameraId, params, kCameraIdParams);
    if (!camera)
    {
        QString errStr;
        if (notFoundCameraId.isNull())
            errStr = lit("Missing 'cameraId' param.");
        else
            errStr = lit("Requested camera %1 not found.").arg(notFoundCameraId);
        result.setError(QnJsonRestResult::InvalidParameter, errStr);
        return CODE_INVALID_PARAMETER;
    }
    const QString cameraId = camera->getId().toString();

    if (camera->getStatus() == Qn::Offline || camera->getStatus() == Qn::Unauthorized)
    {
        result.setError(QnJsonRestResult::InvalidParameter,
            lit("Camera resource '%1' is not ready yet.").arg(cameraId));
        return CODE_INVALID_PARAMETER;
    }

    QnPtzControllerPtr controller = qnPtzPool->controller(camera);
    if (!controller)
    {
        result.setError(QnJsonRestResult::InvalidParameter,
            lit("PTZ is not supported by camera '%1'.").arg(cameraId));
        return CODE_INVALID_PARAMETER;
    }

    if (!checkSequence(sequenceId, sequenceNumber))
        return CODE_OK;

    if (!checkUserAccess(processor->accessRights(), camera, command))
        return nx_http::StatusCode::forbidden;

    switch (command)
    {
        case Qn::ContinuousMovePtzCommand:
            NX_VERBOSE(this, lm("Before execute ContinuousMovePtzCommand. %1").arg(params));
            return execCommandAsync(hash, std::bind(&QnPtzRestHandler::executeContinuousMove, this, controller, params, result));
        case Qn::ContinuousFocusPtzCommand:
            NX_VERBOSE(this, lm("Before execute ContinuousFocusPtzCommand. %1").arg(params));
            return execCommandAsync(hash, std::bind(&QnPtzRestHandler::executeContinuousFocus, this, controller, params, result));

        case Qn::AbsoluteDeviceMovePtzCommand:
        case Qn::AbsoluteLogicalMovePtzCommand: return executeAbsoluteMove(controller, params, result);
        case Qn::ViewportMovePtzCommand:        return executeViewportMove(controller, params, result);
        case Qn::GetDevicePositionPtzCommand:
        case Qn::GetLogicalPositionPtzCommand:  return executeGetPosition(controller, params, result);
        case Qn::CreatePresetPtzCommand:        return executeCreatePreset(controller, params, result);
        case Qn::UpdatePresetPtzCommand:        return executeUpdatePreset(controller, params, result);
        case Qn::RemovePresetPtzCommand:        return executeRemovePreset(controller, params, result);
        case Qn::ActivatePresetPtzCommand:      return executeActivatePreset(controller, params, result);
        case Qn::GetPresetsPtzCommand:          return executeGetPresets(controller, params, result);
        case Qn::CreateTourPtzCommand:          return executeCreateTour(controller, params, body, result);
        case Qn::RemoveTourPtzCommand:          return executeRemoveTour(controller, params, result);
        case Qn::ActivateTourPtzCommand:        return executeActivateTour(controller, params, result);
        case Qn::GetToursPtzCommand:            return executeGetTours(controller, params, result);
        case Qn::GetActiveObjectPtzCommand:     return executeGetActiveObject(controller, params, result);
        case Qn::UpdateHomeObjectPtzCommand:    return executeUpdateHomeObject(controller, params, result);
        case Qn::GetHomeObjectPtzCommand:       return executeGetHomeObject(controller, params, result);
        case Qn::GetAuxilaryTraitsPtzCommand:   return executeGetAuxilaryTraits(controller, params, result);
        case Qn::RunAuxilaryCommandPtzCommand:  return executeRunAuxilaryCommand(controller, params, result);
        case Qn::GetDataPtzCommand:             return executeGetData(controller, params, result);
        default:                                return CODE_INVALID_PARAMETER;
    }
}

int QnPtzRestHandler::executeContinuousMove(const QnPtzControllerPtr &controller, const QnRequestParams &params, QnJsonRestResult &result)
{
    qreal xSpeed, ySpeed, zSpeed;
    if (
        !requireParameter(params, lit("xSpeed"), result, &xSpeed) ||
        !requireParameter(params, lit("ySpeed"), result, &ySpeed) ||
        !requireParameter(params, lit("zSpeed"), result, &zSpeed)
        )
    {
        return CODE_INVALID_PARAMETER;
    }

    QVector3D speed(xSpeed, ySpeed, zSpeed);
    if (!controller->continuousMove(speed))
        return CODE_INTERNAL_ERROR;

    return CODE_OK;
}

int QnPtzRestHandler::executeContinuousFocus(const QnPtzControllerPtr &controller, const QnRequestParams &params, QnJsonRestResult &result)
{
    qreal speed;
    if (!requireParameter(params, lit("speed"), result, &speed))
        return CODE_INVALID_PARAMETER;

    if (!controller->continuousFocus(speed))
        return CODE_INTERNAL_ERROR;

    return CODE_OK;
}

int QnPtzRestHandler::executeAbsoluteMove(const QnPtzControllerPtr &controller, const QnRequestParams &params, QnJsonRestResult &result)
{
    Qn::PtzCommand command;
    qreal xPos, yPos, zPos, speed;
    if (
        !requireParameter(params, lit("command"), result, &command) ||
        !requireParameter(params, lit("xPos"), result, &xPos) ||
        !requireParameter(params, lit("yPos"), result, &yPos) ||
        !requireParameter(params, lit("zPos"), result, &zPos) ||
        !requireParameter(params, lit("speed"), result, &speed)
        )
    {
        return CODE_INVALID_PARAMETER;
    }

    QVector3D position(xPos, yPos, zPos);
    if (!controller->absoluteMove(command == Qn::AbsoluteDeviceMovePtzCommand ? Qn::DevicePtzCoordinateSpace : Qn::LogicalPtzCoordinateSpace, position, speed))
        return CODE_INTERNAL_ERROR;

    return CODE_OK;
}

int QnPtzRestHandler::executeViewportMove(const QnPtzControllerPtr &controller, const QnRequestParams &params, QnJsonRestResult &result)
{
    qreal viewportTop, viewportLeft, viewportBottom, viewportRight, aspectRatio, speed;
    if (
        !requireParameter(params, lit("viewportTop"), result, &viewportTop) ||
        !requireParameter(params, lit("viewportLeft"), result, &viewportLeft) ||
        !requireParameter(params, lit("viewportBottom"), result, &viewportBottom) ||
        !requireParameter(params, lit("viewportRight"), result, &viewportRight) ||
        !requireParameter(params, lit("aspectRatio"), result, &aspectRatio) ||
        !requireParameter(params, lit("speed"), result, &speed)
        )
    {
        return CODE_INVALID_PARAMETER;
    }

    QRectF viewport(QPointF(viewportLeft, viewportTop), QPointF(viewportRight, viewportBottom));
    if (!controller->viewportMove(aspectRatio, viewport, speed))
        return CODE_INTERNAL_ERROR;

    return CODE_OK;
}

int QnPtzRestHandler::executeGetPosition(const QnPtzControllerPtr &controller, const QnRequestParams &params, QnJsonRestResult &result)
{
    Qn::PtzCommand command;
    if (!requireParameter(params, lit("command"), result, &command))
        return CODE_INVALID_PARAMETER;

    QVector3D position;
    if (!controller->getPosition(command == Qn::GetDevicePositionPtzCommand ? Qn::DevicePtzCoordinateSpace : Qn::LogicalPtzCoordinateSpace, &position))
        return CODE_INTERNAL_ERROR;

    result.setReply(position);
    return CODE_OK;
}

int QnPtzRestHandler::executeCreatePreset(const QnPtzControllerPtr &controller, const QnRequestParams &params, QnJsonRestResult &result)
{
    QString presetId, presetName;
    if (!requireParameter(params, lit("presetId"), result, &presetId) || !requireParameter(params, lit("presetName"), result, &presetName))
        return CODE_INVALID_PARAMETER;

    QnPtzPreset preset(presetId, presetName);
    if (!controller->createPreset(preset))
        return CODE_INTERNAL_ERROR;

    return CODE_OK;
}

int QnPtzRestHandler::executeUpdatePreset(const QnPtzControllerPtr &controller, const QnRequestParams &params, QnJsonRestResult &result)
{
    QString presetId, presetName;
    if (!requireParameter(params, lit("presetId"), result, &presetId) || !requireParameter(params, lit("presetName"), result, &presetName))
        return CODE_INVALID_PARAMETER;

    QnPtzPreset preset(presetId, presetName);
    if (!controller->updatePreset(preset))
        return CODE_INTERNAL_ERROR;

    return CODE_OK;
}

int QnPtzRestHandler::executeRemovePreset(const QnPtzControllerPtr &controller, const QnRequestParams &params, QnJsonRestResult &result)
{
    QString presetId;
    if (!requireParameter(params, lit("presetId"), result, &presetId))
        return CODE_INVALID_PARAMETER;

    if (!controller->removePreset(presetId))
        return CODE_INTERNAL_ERROR;

    return CODE_OK;
}

int QnPtzRestHandler::executeActivatePreset(const QnPtzControllerPtr &controller, const QnRequestParams &params, QnJsonRestResult &result)
{
    QString presetId;
    qreal speed;
    if (!requireParameter(params, lit("presetId"), result, &presetId) || !requireParameter(params, lit("speed"), result, &speed))
        return CODE_INVALID_PARAMETER;

    if (!controller->activatePreset(presetId, speed))
        return CODE_INTERNAL_ERROR;

    return CODE_OK;
}

int QnPtzRestHandler::executeGetPresets(const QnPtzControllerPtr &controller, const QnRequestParams &, QnJsonRestResult &result)
{
    QnPtzPresetList presets;
    if (!controller->getPresets(&presets))
        return CODE_INTERNAL_ERROR;

    result.setReply(presets);
    return CODE_OK;
}

int QnPtzRestHandler::executeCreateTour(const QnPtzControllerPtr &controller, const QnRequestParams &, const QByteArray &body, QnJsonRestResult& /*result*/)
{
    QnPtzTour tour;
    if (!QJson::deserialize(body, &tour))
        return CODE_INVALID_PARAMETER;

    // TODO: #Elric use result.

    if (!controller->createTour(tour))
        return CODE_INTERNAL_ERROR;

    return CODE_OK;
}

int QnPtzRestHandler::executeRemoveTour(const QnPtzControllerPtr &controller, const QnRequestParams &params, QnJsonRestResult &result)
{
    QString tourId;
    if (!requireParameter(params, lit("tourId"), result, &tourId))
        return CODE_INVALID_PARAMETER;

    if (!controller->removeTour(tourId))
        return CODE_INTERNAL_ERROR;

    return CODE_OK;
}

int QnPtzRestHandler::executeActivateTour(const QnPtzControllerPtr &controller, const QnRequestParams &params, QnJsonRestResult &result)
{
    QString tourId;
    if (!requireParameter(params, lit("tourId"), result, &tourId))
        return CODE_INVALID_PARAMETER;

    if (!controller->activateTour(tourId))
        return CODE_INTERNAL_ERROR;

    return CODE_OK;
}

int QnPtzRestHandler::executeGetTours(const QnPtzControllerPtr &controller, const QnRequestParams &, QnJsonRestResult &result)
{
    QnPtzTourList tours;
    if (!controller->getTours(&tours))
        return CODE_INTERNAL_ERROR;

    result.setReply(tours);
    return CODE_OK;
}

int QnPtzRestHandler::executeGetActiveObject(const QnPtzControllerPtr &controller, const QnRequestParams &, QnJsonRestResult &result)
{
    QnPtzObject activeObject;
    if (!controller->getActiveObject(&activeObject))
        return CODE_INTERNAL_ERROR;

    result.setReply(activeObject);
    return CODE_OK;
}

int QnPtzRestHandler::executeUpdateHomeObject(const QnPtzControllerPtr &controller, const QnRequestParams &params, QnJsonRestResult &result)
{
    Qn::PtzObjectType objectType;
    QString objectId;
    if (!requireParameter(params, lit("objectType"), result, &objectType) || !requireParameter(params, lit("objectId"), result, &objectId))
        return CODE_INVALID_PARAMETER;

    if (!controller->updateHomeObject(QnPtzObject(objectType, objectId)))
        return CODE_INTERNAL_ERROR;

    return CODE_OK;
}

int QnPtzRestHandler::executeGetHomeObject(const QnPtzControllerPtr &controller, const QnRequestParams &, QnJsonRestResult &result)
{
    QnPtzObject homeObject;
    if (!controller->getHomeObject(&homeObject))
        return CODE_INTERNAL_ERROR;

    result.setReply(homeObject);
    return CODE_OK;
}

int QnPtzRestHandler::executeGetAuxilaryTraits(const QnPtzControllerPtr &controller, const QnRequestParams &params, QnJsonRestResult &result)
{
    Q_UNUSED(params);

    QnPtzAuxilaryTraitList traits;
    if (!controller->getAuxilaryTraits(&traits))
        return CODE_INTERNAL_ERROR;

    result.setReply(traits);
    return CODE_OK;
}

int QnPtzRestHandler::executeRunAuxilaryCommand(const QnPtzControllerPtr &controller, const QnRequestParams &params, QnJsonRestResult &result)
{
    QnPtzAuxilaryTrait trait;
    QString data;
    if (
        !requireParameter(params, lit("trait"), result, &trait) ||
        !requireParameter(params, lit("data"), result, &data)
        )
    {
        return CODE_INVALID_PARAMETER;
    }

    if (!controller->runAuxilaryCommand(trait, data))
        return CODE_INTERNAL_ERROR;

    return CODE_OK;
}

int QnPtzRestHandler::executeGetData(const QnPtzControllerPtr &controller, const QnRequestParams &params, QnJsonRestResult &result)
{
    Qn::PtzDataFields query;
    if (!requireParameter(params, lit("query"), result, &query))
        return CODE_INVALID_PARAMETER;

    QnPtzData data;
    if (!controller->getData(query, &data))
        return CODE_INTERNAL_ERROR;

    result.setReply(data);
    return CODE_OK;
}

// TODO: #Elric not valid anymore
