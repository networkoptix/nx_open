#ifdef ENABLE_ONVIF

#include "onvif_ptz_controller.h"

#include <onvif/soapDeviceBindingProxy.h>

#include <common/common_module.h>
#include <utils/math/fuzzy.h>
#include <core/resource/resource_data.h>
#include <core/resource_management/resource_data_pool.h>
#include <plugins/resources/onvif/onvif_resource.h>

#include "soap_wrapper.h"


// -------------------------------------------------------------------------- //
// QnOnvifPtzController
// -------------------------------------------------------------------------- //
QnOnvifPtzController::QnOnvifPtzController(const QnPlOnvifResourcePtr &resource): 
    base_type(resource),
    m_resource(resource),
    m_capabilities(0)
{
    m_limits.minPan = -1.0;
    m_limits.maxPan = 1.0;
    m_limits.minTilt = -1.0;
    m_limits.maxTilt = 1.0;
    m_limits.minFov = 0.0;
    m_limits.maxFov = 1.0;

    SpeedLimits defaultLimits(-1.0, 1.0);
    m_panSpeedLimits = m_tiltSpeedLimits = m_zoomSpeedLimits = m_focusSpeedLimits = defaultLimits;

    m_capabilities = Qn::ContinuousPtzCapabilities | Qn::AbsolutePtzCapabilities | Qn::DevicePositioningPtzCapability | Qn::LimitsPtzCapability | Qn::FlipPtzCapability;
    if(m_resource->getPtzUrl().isEmpty())
        m_capabilities = Qn::NoPtzCapabilities;

    m_stopBroken = qnCommon->dataPool()->data(resource, true).value<bool>(lit("onvifPtzStopBroken"), false);

    initContinuousMove();
    //initContinuousFocus(); // Disabled for now.

    // TODO: #PTZ #Elric actually implement flip!
}

QnOnvifPtzController::~QnOnvifPtzController() {
    return;
}

Qn::PtzCapabilities QnOnvifPtzController::initContinuousMove() {
    QString ptzUrl = m_resource->getPtzUrl();
    if(ptzUrl.isEmpty())
        return Qn::NoPtzCapabilities;

    QAuthenticator auth = m_resource->getAuth();
    PtzSoapWrapper ptz(ptzUrl.toStdString(), auth.user(), auth.password(), m_resource->getTimeDrift());

    _onvifPtz__GetConfigurations request;
    _onvifPtz__GetConfigurationsResponse response;
    if (ptz.doGetConfigurations(request, response) != SOAP_OK)
        return Qn::NoPtzCapabilities;
    if(response.PTZConfiguration.empty())
        return Qn::NoPtzCapabilities;

    _onvifPtz__GetNode nodeRequest;
    _onvifPtz__GetNodeResponse nodeResponse;
    nodeRequest.NodeToken = response.PTZConfiguration[0]->NodeToken;

    if (ptz.doGetNode(nodeRequest, nodeResponse) != SOAP_OK)
        return Qn::NoPtzCapabilities;

    onvifXsd__PTZNode *ptzNode = nodeResponse.PTZNode;
    if (!ptzNode) 
        return Qn::NoPtzCapabilities;
    onvifXsd__PTZSpaces *spaces = ptzNode->SupportedPTZSpaces;
    if (!spaces)
        return Qn::NoPtzCapabilities;
        
    Qn::PtzCapabilities result = Qn::NoPtzCapabilities;
    if (spaces->ContinuousPanTiltVelocitySpace.size() > 0 && spaces->ContinuousPanTiltVelocitySpace[0]) {
        if (spaces->ContinuousPanTiltVelocitySpace[0]->XRange) {
            m_panSpeedLimits.min = spaces->ContinuousPanTiltVelocitySpace[0]->XRange->Min;
            m_panSpeedLimits.max = spaces->ContinuousPanTiltVelocitySpace[0]->XRange->Max;
            result |= Qn::ContinuousPanCapability;
        }
        if (spaces->ContinuousPanTiltVelocitySpace[0]->YRange) {
            m_tiltSpeedLimits.min = spaces->ContinuousPanTiltVelocitySpace[0]->YRange->Min;
            m_tiltSpeedLimits.max = spaces->ContinuousPanTiltVelocitySpace[0]->YRange->Max;
            result |= Qn::ContinuousTiltCapability;
        }
    }
    if (spaces->ContinuousZoomVelocitySpace.size() > 0 && spaces->ContinuousZoomVelocitySpace[0]) {
        if (spaces->ContinuousZoomVelocitySpace[0]->XRange) {
            m_zoomSpeedLimits.min = spaces->ContinuousZoomVelocitySpace[0]->XRange->Min;
            m_zoomSpeedLimits.max = spaces->ContinuousZoomVelocitySpace[0]->XRange->Max;
            result |= Qn::ContinuousZoomCapability;
        }
    }
    return result;
}

Qn::PtzCapabilities QnOnvifPtzController::initContinuousFocus() {
    QString imagingUrl = m_resource->getImagingUrl();
    if(imagingUrl.isEmpty())
        return Qn::NoPtzCapabilities;

    QAuthenticator auth = m_resource->getAuth();
    ImagingSoapWrapper imaging(imagingUrl.toStdString(), auth.user(), auth.password(), m_resource->getTimeDrift());

    _onvifImg__GetMoveOptions moveOptionsRequest;
    moveOptionsRequest.VideoSourceToken = m_resource->getVideoSourceToken().toStdString();

    _onvifImg__GetMoveOptionsResponse moveOptionsResponse;
    if (imaging.getMoveOptions(moveOptionsRequest, moveOptionsResponse) != SOAP_OK)
        return Qn::NoPtzCapabilities;

    onvifXsd__MoveOptions20 *moveOptions = moveOptionsResponse.MoveOptions;
    if(!moveOptions)
        return Qn::NoPtzCapabilities;

    onvifXsd__ContinuousFocusOptions *continuousFocusOptions = moveOptions->Continuous;
    if(!continuousFocusOptions)
        return Qn::NoPtzCapabilities;

    onvifXsd__FloatRange *speedLimits = continuousFocusOptions->Speed;
    if(!speedLimits)
        return Qn::NoPtzCapabilities;

    m_focusSpeedLimits.min = speedLimits->Min;
    m_focusSpeedLimits.max = speedLimits->Max;
    return Qn::ContinuousFocusCapability;
}

Qn::PtzCapabilities QnOnvifPtzController::getCapabilities() {
    return m_capabilities;
}

double QnOnvifPtzController::normalizeSpeed(qreal speed, const SpeedLimits &speedLimits) {
    speed *= speed >= 0 ? speedLimits.max : -speedLimits.min;
    return qBound(speedLimits.min, speed, speedLimits.max);
}

bool QnOnvifPtzController::stopInternal() {
    QString ptzUrl = m_resource->getPtzUrl();
    if(ptzUrl.isEmpty())
        return false;

    QAuthenticator auth(m_resource->getAuth());
    PtzSoapWrapper ptz (ptzUrl.toStdString(), auth.user(), auth.password(), m_resource->getTimeDrift());

    bool stopValue = true;

    _onvifPtz__Stop request;
    request.ProfileToken = m_resource->getPtzProfileToken().toStdString();
    request.PanTilt = &stopValue;
    request.Zoom = &stopValue;

    _onvifPtz__StopResponse response;
    if (ptz.doStop(request, response) != SOAP_OK) {
        qnWarning("Execution of PTZ stop command for resource '%1' has failed with error %2.", m_resource->getName(), ptz.getLastError());
        return false;
    }
    
    return true;
}

bool QnOnvifPtzController::moveInternal(const QVector3D &speed) {
    QString ptzUrl = m_resource->getPtzUrl();
    if(ptzUrl.isEmpty())
        return false;

    QAuthenticator auth(m_resource->getAuth());
    PtzSoapWrapper ptz (ptzUrl.toStdString(), auth.user(), auth.password(), m_resource->getTimeDrift());

    QVector3D localSpeed = speed;

    onvifXsd__Vector2D onvifPanTiltSpeed;
    onvifPanTiltSpeed.x = normalizeSpeed(speed.x(), m_panSpeedLimits);
    onvifPanTiltSpeed.y = normalizeSpeed(speed.y(), m_tiltSpeedLimits);

    onvifXsd__Vector1D onvifZoomSpeed;
    onvifZoomSpeed.x = normalizeSpeed(speed.z(), m_zoomSpeedLimits);

    onvifXsd__PTZSpeed onvifSpeed;
    onvifSpeed.PanTilt = &onvifPanTiltSpeed;
    onvifSpeed.Zoom = &onvifZoomSpeed;

    _onvifPtz__ContinuousMove request;
    request.ProfileToken = m_resource->getPtzProfileToken().toStdString();
    request.Velocity = &onvifSpeed;

    _onvifPtz__ContinuousMoveResponse response;
    if (ptz.doContinuousMove(request, response) != SOAP_OK) {
        qnWarning("Execution of PTZ continuous move command for resource '%1' has failed with error %2.", m_resource->getName(), ptz.getLastError());
        return false;
    }

    return true;
}

bool QnOnvifPtzController::continuousMove(const QVector3D &speed) {
    if(qFuzzyIsNull(speed) && !m_stopBroken) {
        return stopInternal();
    } else {
        return moveInternal(speed);
    }
}

bool QnOnvifPtzController::continuousFocus(qreal speed) {
    QString imagingUrl = m_resource->getImagingUrl();
    if(imagingUrl.isEmpty())
        return false;

    QAuthenticator auth(m_resource->getAuth());
    ImagingSoapWrapper imaging(imagingUrl.toStdString(), auth.user(), auth.password(), m_resource->getTimeDrift());

    onvifXsd__ContinuousFocus onvifContinuousFocus;
    onvifContinuousFocus.Speed = normalizeSpeed(speed, m_focusSpeedLimits);
    
    onvifXsd__FocusMove onvifFocus;
    onvifFocus.Continuous = &onvifContinuousFocus;

    _onvifImg__Move request;
    request.VideoSourceToken = m_resource->getVideoSourceToken().toStdString();
    request.Focus = &onvifFocus;

    _onvifImg__MoveResponse response;
    if (imaging.move(request, response) != SOAP_OK) {
        qnWarning("Execution of PTZ continuous focus command for resource '%1' has failed with error %2.", m_resource->getName(), imaging.getLastError());
        return false;
    }

    return true;
}

bool QnOnvifPtzController::absoluteMove(Qn::PtzCoordinateSpace space, const QVector3D &position, qreal speed) {
    if(space != Qn::DevicePtzCoordinateSpace)
        return false;

    QString ptzUrl = m_resource->getPtzUrl();
    if(ptzUrl.isEmpty())
        return false;

    QAuthenticator auth(m_resource->getAuth());
    PtzSoapWrapper ptz (ptzUrl.toStdString(), auth.user(), auth.password(), m_resource->getTimeDrift());
    
    onvifXsd__Vector2D onvifPanTilt;
    onvifPanTilt.x = position.x();
    onvifPanTilt.y = position.y();

    onvifXsd__Vector1D onvifZoom;
    onvifZoom.x = position.z();

    onvifXsd__PTZVector onvifPosition;
    onvifPosition.PanTilt = &onvifPanTilt;
    onvifPosition.Zoom = &onvifZoom;

    onvifXsd__Vector2D onvifPanTiltSpeed;
    onvifPanTiltSpeed.x = speed;
    onvifPanTiltSpeed.y = speed;

    onvifXsd__Vector1D onvifZoomSpeed;
    onvifZoomSpeed.x = speed;

    onvifXsd__PTZSpeed onvifSpeed;
    onvifSpeed.PanTilt = &onvifPanTiltSpeed;
    onvifSpeed.Zoom = &onvifZoomSpeed;

    _onvifPtz__AbsoluteMove request;
    request.ProfileToken = m_resource->getPtzProfileToken().toStdString();
    request.Position = &onvifPosition;
    request.Speed = &onvifSpeed;

#if 0
    qDebug() << "";
    qDebug() << "";
    qDebug() << "";
    qDebug() << "ABSOLUTE MOVE" << position;
    qDebug() << "";
    qDebug() << "";
    qDebug() << "";
#endif

    _onvifPtz__AbsoluteMoveResponse response;
    if (ptz.doAbsoluteMove(request, response) != SOAP_OK) {
        qnWarning("Execution of PTZ absolute move command for resource '%1' has failed with error %2.", m_resource->getName(), ptz.getLastError());
        return false;
    }

    return true;
}

bool QnOnvifPtzController::getPosition(Qn::PtzCoordinateSpace space, QVector3D *position) {
    if(space != Qn::DevicePtzCoordinateSpace)
        return false;

    QString ptzUrl = m_resource->getPtzUrl();
    if(ptzUrl.isEmpty())
        return false;

    QAuthenticator auth(m_resource->getAuth());
    PtzSoapWrapper ptz (ptzUrl.toStdString(), auth.user(), auth.password(), m_resource->getTimeDrift());
    
    _onvifPtz__GetStatus request;
    request.ProfileToken = m_resource->getPtzProfileToken().toStdString();

    _onvifPtz__GetStatusResponse response;
    if (ptz.doGetStatus(request, response) != SOAP_OK) {
        qnWarning("Execution of PTZ status command for resource '%1' has failed with error %2.", m_resource->getName(), ptz.getLastError());
        return false;
    }

    *position = QVector3D();

    if (response.PTZStatus && response.PTZStatus->Position) {
        if(response.PTZStatus->Position->PanTilt) {
            position->setX(response.PTZStatus->Position->PanTilt->x);
            position->setY(response.PTZStatus->Position->PanTilt->y);
        }
        if(response.PTZStatus->Position->Zoom) {
            position->setZ(response.PTZStatus->Position->Zoom->x);
        }
    }

    return true;
}

bool QnOnvifPtzController::getLimits(Qn::PtzCoordinateSpace space, QnPtzLimits *limits) {
    if(space != Qn::DevicePtzCoordinateSpace)
        return false;

    *limits = m_limits;
    return true;
}

bool QnOnvifPtzController::getFlip(Qt::Orientations *flip) {
    *flip = 0; // TODO: #PTZ #Elric
    return true;
}

#endif //ENABLE_ONVIF
