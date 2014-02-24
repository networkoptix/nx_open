#include "ptz_handler.h"

#include <utils/common/json.h>
#include <utils/common/lexical.h>
#include <utils/network/tcp_connection_priv.h>

#include <core/resource_management/resource_pool.h>
#include <core/resource/camera_resource.h>
#include <core/ptz/abstract_ptz_controller.h>
#include <core/ptz/ptz_data.h>
#include <core/ptz/ptz_controller_pool.h>

static const int OLD_SEQUENCE_THRESHOLD = 1000 * 60 * 5;


QnPtzHandler::QnPtzHandler() {
    return;
}

void QnPtzHandler::cleanupOldSequence()
{
    QMap<QString, SequenceInfo>::iterator itr = m_sequencedRequests.begin();
    while ( itr != m_sequencedRequests.end())
    {
        SequenceInfo& info = itr.value();
        if (info.m_timer.elapsed() > OLD_SEQUENCE_THRESHOLD)
            itr = m_sequencedRequests.erase(itr);
        else
            ++itr;
    }
}

bool QnPtzHandler::checkSequence(const QString& id, int sequence)
{
    QMutexLocker lock(&m_sequenceMutex);
    cleanupOldSequence();
    if (id.isEmpty() || sequence == -1)
        return true; // do not check if empty

    if (m_sequencedRequests[id].sequence > sequence)
        return false;

    m_sequencedRequests[id] = SequenceInfo(sequence);
    return true;
}

int QnPtzHandler::executePost(const QString &, const QnRequestParams &params, const QByteArray &body, QnJsonRestResult &result) {
    QString sequenceId;
    int sequenceNumber = -1;
    Qn::PtzCommand command;
    QString resourceId;
    if(
        !requireParameter(params, lit("command"), result, &command) || 
        !requireParameter(params, lit("resourceId"), result, &resourceId) ||
        !requireParameter(params, lit("sequenceId"), result, &sequenceId, true) ||
        !requireParameter(params, lit("sequenceNumber"), result, &sequenceNumber, true)
    ) {
        return CODE_INVALID_PARAMETER;
    }

    QnVirtualCameraResourcePtr camera = qnResPool->getNetResourceByPhysicalId(resourceId).dynamicCast<QnVirtualCameraResource>();
    if(!camera) {
        result.setError(QnJsonRestResult::InvalidParameter, lit("Camera resource '%1' not found.").arg(resourceId));
        return CODE_INVALID_PARAMETER;
    }

    if (camera->getStatus() == QnResource::Offline || camera->getStatus() == QnResource::Unauthorized) {
        result.setError(QnJsonRestResult::InvalidParameter, lit("Camera resource '%1' is not ready yet.").arg(resourceId));
        return CODE_INVALID_PARAMETER;
    }

    QnPtzControllerPtr controller = qnPtzPool->controller(camera);
    if (!controller) {
        result.setError(QnJsonRestResult::InvalidParameter, lit("PTZ is not supported by camera '%1'.").arg(resourceId));
        return CODE_INVALID_PARAMETER;
    }

    if(!checkSequence(sequenceId, sequenceNumber))
        return CODE_OK;

    switch(command) {
    case Qn::ContinuousMovePtzCommand:      return executeContinuousMove(controller, params, result);
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
    case Qn::GetDataPtzCommand:             return executeGetData(controller, params, result);
    default:                                return CODE_INVALID_PARAMETER;
    }
}

int QnPtzHandler::executeContinuousMove(const QnPtzControllerPtr &controller, const QnRequestParams &params, QnJsonRestResult &result) {
    qreal xSpeed, ySpeed, zSpeed;
    if(
        !requireParameter(params, lit("xSpeed"), result, &xSpeed) || 
        !requireParameter(params, lit("ySpeed"), result, &ySpeed) || 
        !requireParameter(params, lit("zSpeed"), result, &zSpeed)
    ) {
        return CODE_INVALID_PARAMETER;
    }

    QVector3D speed(xSpeed, ySpeed, zSpeed);
    if(!controller->continuousMove(speed))
        return CODE_INTERNAL_ERROR;

    return CODE_OK;
}

int QnPtzHandler::executeAbsoluteMove(const QnPtzControllerPtr &controller, const QnRequestParams &params, QnJsonRestResult &result) {
    Qn::PtzCommand command;
    qreal xPos, yPos, zPos, speed;
    if(
        !requireParameter(params, lit("command"), result, &command) || 
        !requireParameter(params, lit("xPos"), result, &xPos) || 
        !requireParameter(params, lit("yPos"), result, &yPos) || 
        !requireParameter(params, lit("zPos"), result, &zPos) ||
        !requireParameter(params, lit("speed"), result, &speed)
    ) {
        return CODE_INVALID_PARAMETER;
    }
    
    QVector3D position(xPos, yPos, zPos);
    if(!controller->absoluteMove(command == Qn::AbsoluteDeviceMovePtzCommand ? Qn::DevicePtzCoordinateSpace : Qn::LogicalPtzCoordinateSpace, position, speed))
        return CODE_INTERNAL_ERROR;

    return CODE_OK;
}

int QnPtzHandler::executeViewportMove(const QnPtzControllerPtr &controller, const QnRequestParams &params, QnJsonRestResult &result) {
    qreal viewportTop, viewportLeft, viewportBottom, viewportRight, aspectRatio, speed;
    if(
        !requireParameter(params, lit("viewportTop"),       result, &viewportTop) || 
        !requireParameter(params, lit("viewportLeft"),      result, &viewportLeft) || 
        !requireParameter(params, lit("viewportBottom"),    result, &viewportBottom) ||
        !requireParameter(params, lit("viewportRight"),     result, &viewportRight) || 
        !requireParameter(params, lit("aspectRatio"),       result, &aspectRatio) ||
        !requireParameter(params, lit("speed"),             result, &speed)
    ) {
        return CODE_INVALID_PARAMETER;
    }
    
    QRectF viewport(QPointF(viewportLeft, viewportTop), QPointF(viewportRight, viewportBottom));
    if(!controller->viewportMove(aspectRatio, viewport, speed))
        return CODE_INTERNAL_ERROR;

    return CODE_OK;
}

int QnPtzHandler::executeGetPosition(const QnPtzControllerPtr &controller, const QnRequestParams &params, QnJsonRestResult &result) {
    Qn::PtzCommand command;
    if(!requireParameter(params, lit("command"), result, &command))
        return CODE_INVALID_PARAMETER;

    QVector3D position;
    if(!controller->getPosition(command == Qn::GetDevicePositionPtzCommand ? Qn::DevicePtzCoordinateSpace : Qn::LogicalPtzCoordinateSpace, &position))
        return CODE_INTERNAL_ERROR;

    result.setReply(position);
    return CODE_OK;
}

int QnPtzHandler::executeCreatePreset(const QnPtzControllerPtr &controller, const QnRequestParams &params, QnJsonRestResult &result) {
    QString presetId, presetName;
    if(!requireParameter(params, lit("presetId"), result, &presetId) || !requireParameter(params, lit("presetName"), result, &presetName))
        return CODE_INVALID_PARAMETER;

    QnPtzPreset preset(presetId, presetName);
    if(!controller->createPreset(preset))
        return CODE_INTERNAL_ERROR;

    return CODE_OK;
}

int QnPtzHandler::executeUpdatePreset(const QnPtzControllerPtr &controller, const QnRequestParams &params, QnJsonRestResult &result) {
    QString presetId, presetName;
    if(!requireParameter(params, lit("presetId"), result, &presetId) || !requireParameter(params, lit("presetName"), result, &presetName))
        return CODE_INVALID_PARAMETER;

    QnPtzPreset preset(presetId, presetName);
    if(!controller->updatePreset(preset))
        return CODE_INTERNAL_ERROR;

    return CODE_OK;
}

int QnPtzHandler::executeRemovePreset(const QnPtzControllerPtr &controller, const QnRequestParams &params, QnJsonRestResult &result) {
    QString presetId;
    if(!requireParameter(params, lit("presetId"), result, &presetId))
        return CODE_INVALID_PARAMETER;

    if(!controller->removePreset(presetId))
        return CODE_INTERNAL_ERROR;

    return CODE_OK;
}

int QnPtzHandler::executeActivatePreset(const QnPtzControllerPtr &controller, const QnRequestParams &params, QnJsonRestResult &result) {
    QString presetId;
    qreal speed;
    if(!requireParameter(params, lit("presetId"), result, &presetId) || !requireParameter(params, lit("speed"), result, &speed))
        return CODE_INVALID_PARAMETER;

    if(!controller->activatePreset(presetId, speed))
        return CODE_INTERNAL_ERROR;

    return CODE_OK;
}

int QnPtzHandler::executeGetPresets(const QnPtzControllerPtr &controller, const QnRequestParams &, QnJsonRestResult &result) {
    QnPtzPresetList presets;
    if(!controller->getPresets(&presets))
        return CODE_INTERNAL_ERROR;

    result.setReply(presets);
    return CODE_OK;
}

int QnPtzHandler::executeCreateTour(const QnPtzControllerPtr &controller, const QnRequestParams &, const QByteArray &body, QnJsonRestResult &result) {
    QnPtzTour tour;
    if(!QJson::deserialize(body, &tour))
        return CODE_INVALID_PARAMETER;

    // TODO: #Elric use result.

    if(!controller->createTour(tour))
        return CODE_INTERNAL_ERROR;

    return CODE_OK;
}

int QnPtzHandler::executeRemoveTour(const QnPtzControllerPtr &controller, const QnRequestParams &params, QnJsonRestResult &result) {
    QString tourId;
    if(!requireParameter(params, lit("tourId"), result, &tourId))
        return CODE_INVALID_PARAMETER;

    if(!controller->removeTour(tourId))
        return CODE_INTERNAL_ERROR;

    return CODE_OK;
}

int QnPtzHandler::executeActivateTour(const QnPtzControllerPtr &controller, const QnRequestParams &params, QnJsonRestResult &result) {
    QString tourId;
    if(!requireParameter(params, lit("tourId"), result, &tourId))
        return CODE_INVALID_PARAMETER;

    if(!controller->activateTour(tourId))
        return CODE_INTERNAL_ERROR;

    return CODE_OK;
}

int QnPtzHandler::executeGetTours(const QnPtzControllerPtr &controller, const QnRequestParams &, QnJsonRestResult &result) {
    QnPtzTourList tours;
    if(!controller->getTours(&tours))
        return CODE_INTERNAL_ERROR;

    result.setReply(tours);
    return CODE_OK;
}

int QnPtzHandler::executeGetActiveObject(const QnPtzControllerPtr &controller, const QnRequestParams &, QnJsonRestResult &result) {
    QnPtzObject activeObject;
    if(!controller->getActiveObject(&activeObject))
        return CODE_INTERNAL_ERROR;

    result.setReply(activeObject);
    return CODE_OK;
}

int QnPtzHandler::executeUpdateHomeObject(const QnPtzControllerPtr &controller, const QnRequestParams &params, QnJsonRestResult &result) {
    Qn::PtzObjectType objectType;
    QString objectId;
    if(!requireParameter(params, lit("objectType"), result, &objectType) || !requireParameter(params, lit("objectId"), result, &objectId))
        return CODE_INVALID_PARAMETER;

    if(!controller->updateHomeObject(QnPtzObject(objectType, objectId)))
        return CODE_INTERNAL_ERROR;

    return CODE_OK;
}

int QnPtzHandler::executeGetHomeObject(const QnPtzControllerPtr &controller, const QnRequestParams &, QnJsonRestResult &result) {
    QnPtzObject homeObject;
    if(!controller->getHomeObject(&homeObject))
        return CODE_INTERNAL_ERROR;

    result.setReply(homeObject);
    return CODE_OK;
}

int QnPtzHandler::executeGetData(const QnPtzControllerPtr &controller, const QnRequestParams &params, QnJsonRestResult &result) {
    Qn::PtzDataFields query;
    if(!requireParameter(params, lit("query"), result, &query))
        return CODE_INVALID_PARAMETER;

    QnPtzData data;
    if(!controller->getData(query, &data))
        return CODE_INTERNAL_ERROR;

    result.setReply(data);
    return CODE_OK;
}

QString QnPtzHandler::description() const
{
    return "\
        There are several ptz commands: <BR>\
        <b>api/ptz/move</b> - start camera moving.<BR>\
        <b>api/ptz/moveTo</b> - go to absolute position.<BR>\
        <b>api/ptz/stop</b> - stop camera moving.<BR>\
        <b>api/ptz/getPosition</b> - return current camera position.<BR>\
        <b>api/ptz/getSpaceMapper</b> - return JSON-serialized PTZ space mapper for the given camera, if any.<BR>\
        <b>api/ptz/calibrate</b> - calibrate moving speed (addition speed coeff).<BR>\
        <b>api/ptz/getCalibrate</b> - read current calibration settings.<BR>\
        <BR>\
        Param <b>res_id</b> - camera physicalID.<BR>\
        <BR>\
        Arguments for 'move' and 'calibrate' commands:<BR>\
        Param <b>xSpeed</b> - rotation X velocity in range [-1..+1].<BR>\
        Param <b>ySpeed</b> - rotation Y velocity in range [-1..+1].<BR>\
        Param <b>zoomSpeed</b> - zoom velocity in range [-1..+1].<BR>\
        <BR>\
        Arguments for 'moveTo' commands:<BR>\
        Param <b>xPos</b> - go to absolute X position in range [-1..+1].<BR>\
        Param <b>yPos</b> - go to absolute Y position in range [-1..+1].<BR>\
        Param <b>zoomPos</b> - Optional. Go to absolute zoom position in range [0..+1].<BR>\
        <BR>\
        If PTZ command do not return data, function return simple 'OK' message on success or error message if command fail. \
        For 'getCalibrate' command returns XML with coeffecients. For 'getPosition' command returns XML with current position.\
    ";
}
