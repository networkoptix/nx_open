#ifdef ENABLE_ONVIF

#include "onvif_ptz_controller.h"

#include <onvif/soapDeviceBindingProxy.h>

#include <common/common_module.h>
#include <utils/math/fuzzy.h>
#include <core/resource/resource_data.h>
#include <core/resource_management/resource_data_pool.h>
#include <plugins/resource/onvif/onvif_resource.h>

#include "soap_wrapper.h"

static QByteArray ENCODE_PREFIX("BASE64_");

static std::string toLatinStdString(const QString& value)
{
    std::string value1 = value.toStdString();
    QByteArray latinString = value.toLatin1();
    std::string value2(latinString.constData(), latinString.length());
    if (value1 == value2)
        return value1;
    else {
        QByteArray result = ENCODE_PREFIX.append(value.toUtf8().toBase64());
        return std::string(result.constData(), result.length());
    }
}

static QString fromLatinStdString(const std::string& value)
{
    QByteArray data(value.c_str());
    if (data.startsWith(ENCODE_PREFIX)) {
        data = QByteArray::fromBase64(data.mid(ENCODE_PREFIX.length()));
        return QString::fromUtf8(data);
    }
    else {
        return QString::fromStdString(value);
    }
}



// -------------------------------------------------------------------------- //
// QnOnvifPtzController
// -------------------------------------------------------------------------- //
QnOnvifPtzController::QnOnvifPtzController(const QnPlOnvifResourcePtr &resource): 
    base_type(resource),
    m_resource(resource),
    m_capabilities(Qn::NoPtzCapabilities),
    m_stopBroken(false)
{
    m_limits.minPan = -1.0;
    m_limits.maxPan = 1.0;
    m_limits.minTilt = -1.0;
    m_limits.maxTilt = 1.0;
    m_limits.minFov = 0.0;
    m_limits.maxFov = 1.0;

    SpeedLimits defaultLimits(-1.0, 1.0);
    m_panSpeedLimits = m_tiltSpeedLimits = m_zoomSpeedLimits = m_focusSpeedLimits = defaultLimits;

    QnResourceData data = qnCommon->dataPool()->data(resource, true);
    m_stopBroken            = data.value<bool>(lit("onvifPtzStopBroken"),           false);
    bool absoluteMoveBroken = data.value<bool>(lit("onvifPtzAbsoluteMoveBroken"),   false);
    bool focusEnabled       = data.value<bool>(lit("onvifPtzFocusEnabled"),         false);
    bool presetsEnabled     = data.value<bool>(lit("onvifPtzPresetsEnabled"),       false);

    m_capabilities |= initMove();
    if(absoluteMoveBroken)
        m_capabilities &= ~(Qn::AbsolutePtzCapabilities | Qn::DevicePositioningPtzCapability);
    if(focusEnabled)
        m_capabilities |= initContinuousFocus();
    if(presetsEnabled)
        m_capabilities |= initPresets();

    // TODO: #PTZ #Elric actually implement flip!
}

QnOnvifPtzController::~QnOnvifPtzController() {
    return;
}

Qn::PtzCapabilities QnOnvifPtzController::initMove() {
    QString ptzUrl = m_resource->getPtzUrl();
    if(ptzUrl.isEmpty())
        return Qn::NoPtzCapabilities;

    QAuthenticator auth = m_resource->getAuth();
    PtzSoapWrapper ptz(ptzUrl.toStdString(), auth.user(), auth.password(), m_resource->getTimeDrift());

    _onvifPtz__GetConfigurations request;
    _onvifPtz__GetConfigurationsResponse response;
    if (ptz.doGetConfigurations(request, response) != SOAP_OK)
        return Qn::NoPtzCapabilities;
    if(response.PTZConfiguration.empty() || !response.PTZConfiguration[0])
        return Qn::NoPtzCapabilities;

	// TODO: #PTZ #Elric we can init caps by examining spaces in response!

    Qn::PtzCapabilities configCapabilities = Qn::NoPtzCapabilities;
    if(response.PTZConfiguration[0]->DefaultContinuousPanTiltVelocitySpace)
        configCapabilities |= Qn::ContinuousPanCapability | Qn::ContinuousTiltCapability;
    if(response.PTZConfiguration[0]->DefaultContinuousZoomVelocitySpace)
        configCapabilities |= Qn::ContinuousZoomCapability;
    if(response.PTZConfiguration[0]->DefaultAbsolutePantTiltPositionSpace)
	    configCapabilities |= Qn::AbsolutePanCapability | Qn::AbsoluteTiltCapability;
    if(response.PTZConfiguration[0]->DefaultAbsoluteZoomPositionSpace)
        configCapabilities |= Qn::AbsoluteZoomCapability;

    _onvifPtz__GetNode nodeRequest;
    _onvifPtz__GetNodeResponse nodeResponse;
    nodeRequest.NodeToken = response.PTZConfiguration[0]->NodeToken;
    if (ptz.doGetNode(nodeRequest, nodeResponse) != SOAP_OK)
        return Qn::NoPtzCapabilities;

    if (!nodeResponse.PTZNode || !nodeResponse.PTZNode->SupportedPTZSpaces) 
        return Qn::NoPtzCapabilities;
    onvifXsd__PTZSpaces *spaces = nodeResponse.PTZNode->SupportedPTZSpaces;
        
    Qn::PtzCapabilities nodeCapabilities = Qn::NoPtzCapabilities;
    if(!spaces->ContinuousPanTiltVelocitySpace.empty() && spaces->ContinuousPanTiltVelocitySpace[0]) {
        if(spaces->ContinuousPanTiltVelocitySpace[0]->XRange) {
            m_panSpeedLimits.min = spaces->ContinuousPanTiltVelocitySpace[0]->XRange->Min;
            m_panSpeedLimits.max = spaces->ContinuousPanTiltVelocitySpace[0]->XRange->Max;
            nodeCapabilities |= Qn::ContinuousPanCapability;
        }
        if(spaces->ContinuousPanTiltVelocitySpace[0]->YRange) {
            m_tiltSpeedLimits.min = spaces->ContinuousPanTiltVelocitySpace[0]->YRange->Min;
            m_tiltSpeedLimits.max = spaces->ContinuousPanTiltVelocitySpace[0]->YRange->Max;
            nodeCapabilities |= Qn::ContinuousTiltCapability;
        }
    }
    if(!spaces->ContinuousZoomVelocitySpace.empty() && spaces->ContinuousZoomVelocitySpace[0]) {
        if(spaces->ContinuousZoomVelocitySpace[0]->XRange) {
            m_zoomSpeedLimits.min = spaces->ContinuousZoomVelocitySpace[0]->XRange->Min;
            m_zoomSpeedLimits.max = spaces->ContinuousZoomVelocitySpace[0]->XRange->Max;
            nodeCapabilities |= Qn::ContinuousZoomCapability;
        }
    }
    if(!spaces->AbsolutePanTiltPositionSpace.empty() && spaces->AbsolutePanTiltPositionSpace[0]) {
        if(spaces->AbsolutePanTiltPositionSpace[0]->XRange) {
            m_limits.minPan = spaces->AbsolutePanTiltPositionSpace[0]->XRange->Min;
            m_limits.maxPan = spaces->AbsolutePanTiltPositionSpace[0]->XRange->Max;
            nodeCapabilities |= Qn::AbsolutePanCapability;
        }
        if(spaces->AbsolutePanTiltPositionSpace[0]->YRange) {
            m_limits.minTilt = spaces->AbsolutePanTiltPositionSpace[0]->YRange->Min;
            m_limits.maxTilt = spaces->AbsolutePanTiltPositionSpace[0]->YRange->Max;
            nodeCapabilities |= Qn::AbsoluteTiltCapability;
        }
    }
    if(!spaces->AbsoluteZoomPositionSpace.empty() && spaces->AbsoluteZoomPositionSpace[0]) {
        if(spaces->AbsoluteZoomPositionSpace[0]->XRange) {
            m_limits.minFov = spaces->AbsoluteZoomPositionSpace[0]->XRange->Min;
            m_limits.maxFov = spaces->AbsoluteZoomPositionSpace[0]->XRange->Max;
            nodeCapabilities |= Qn::AbsoluteZoomCapability;
        }
    }

    Qn::PtzCapabilities result = configCapabilities & nodeCapabilities;
    if(result & Qn::AbsolutePtzCapabilities) {
        result |= Qn::DevicePositioningPtzCapability;

        if(nodeCapabilities & Qn::AbsolutePtzCapabilities)
            result |= Qn::LimitsPtzCapability;
    }
        
    return result;
}

Qn::PtzCapabilities QnOnvifPtzController::initPresets() {
    QString ptzUrl = m_resource->getPtzUrl();
    if(ptzUrl.isEmpty())
        return Qn::NoPtzCapabilities;

    QAuthenticator auth = m_resource->getAuth();
    PtzSoapWrapper ptz(ptzUrl.toStdString(), auth.user(), auth.password(), m_resource->getTimeDrift());

    GetPresetsReq request;
    request.ProfileToken = m_resource->getPtzProfileToken().toStdString();
    GetPresetsResp response;
    if (ptz.getPresets(request, response) != SOAP_OK)
        return Qn::NoPtzCapabilities;

    m_presetNameByToken.clear();
    for(onvifXsd__PTZPreset *preset: response.Preset) {
        if (preset) {
            QString id = QString::fromStdString(*preset->token);
            QString name = fromLatinStdString(*preset->Name);
            m_presetNameByToken.insert(id, name);
        }
    }
    
    return Qn::PresetsPtzCapability;
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

    if(!moveOptionsResponse.MoveOptions || !moveOptionsResponse.MoveOptions->Continuous || !moveOptionsResponse.MoveOptions->Continuous->Speed)
        return Qn::NoPtzCapabilities;
    onvifXsd__FloatRange *speedLimits = moveOptionsResponse.MoveOptions->Continuous->Speed;

    m_focusSpeedLimits.min = speedLimits->Min;
    m_focusSpeedLimits.max = speedLimits->Max;
    return Qn::ContinuousFocusCapability;
}

double QnOnvifPtzController::normalizeSpeed(qreal speed, const SpeedLimits &speedLimits) {
    speed *= speed >= 0 ? speedLimits.max : -speedLimits.min;
    return qBound(speedLimits.min, speed, speedLimits.max);
}

Qn::PtzCapabilities QnOnvifPtzController::getCapabilities() {
    return m_capabilities;
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
    onvifPanTiltSpeed.x = speed; // TODO: #Elric #PTZ do we need to adjust speed to speed limits here?
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

QString QnOnvifPtzController::presetToken(const QString &presetId) {
    QString internalId = m_presetTokenById.value(presetId);
    if (!internalId.isEmpty())
        return internalId;
    else
        return presetId;
}

QString QnOnvifPtzController::presetName(const QString &presetId) {
    QString internalId = m_presetTokenById.value(presetId);
    if (internalId.isEmpty())
        internalId = presetId;
    return m_presetNameByToken.value(internalId);
}

bool QnOnvifPtzController::removePreset(const QString &presetId) {
    QString ptzUrl = m_resource->getPtzUrl();
    if(ptzUrl.isEmpty())
        return false;

    QAuthenticator auth(m_resource->getAuth());
    PtzSoapWrapper ptz (ptzUrl.toStdString(), auth.user(), auth.password(), m_resource->getTimeDrift());

    RemovePresetReq request;
    RemovePresetResp response;
    request.ProfileToken = m_resource->getPtzProfileToken().toStdString();
    request.PresetToken = presetToken(presetId).toStdString();
    if (ptz.removePreset(request, response) != SOAP_OK) {
        qnWarning("Execution of PTZ remove preset command for resource '%1' has failed with error %2.", m_resource->getName(), ptz.getLastError());
        return false;
    }

    return true;
}

bool QnOnvifPtzController::getPresets(QnPtzPresetList *presets) {
    for (auto itr = m_presetNameByToken.begin(); itr != m_presetNameByToken.end(); ++itr)
        presets->push_back(QnPtzPreset(itr.key(), itr.value()));
    return true;
}

bool QnOnvifPtzController::activatePreset(const QString &presetId, qreal speed) {
    QString ptzUrl = m_resource->getPtzUrl();
    if(ptzUrl.isEmpty())
        return false;

    QAuthenticator auth(m_resource->getAuth());
    PtzSoapWrapper ptz (ptzUrl.toStdString(), auth.user(), auth.password(), m_resource->getTimeDrift());

    onvifXsd__Vector2D onvifPanTiltSpeed;
    onvifPanTiltSpeed.x = speed; // TODO: #Elric #PTZ do we need to adjust speed to speed limits here?
    onvifPanTiltSpeed.y = speed;

    onvifXsd__Vector1D onvifZoomSpeed;
    onvifZoomSpeed.x = speed;

    onvifXsd__PTZSpeed onvifSpeed;
    onvifSpeed.PanTilt = &onvifPanTiltSpeed;
    onvifSpeed.Zoom = &onvifZoomSpeed;

    GotoPresetReq request;
    GotoPresetResp response;
    request.ProfileToken = m_resource->getPtzProfileToken().toStdString();
    request.PresetToken = presetToken(presetId).toStdString();
    request.Speed = &onvifSpeed;

    if (ptz.gotoPreset(request, response) != SOAP_OK) {
        qnWarning("Execution of PTZ goto preset command for resource '%1' has failed with error %2.", m_resource->getName(), ptz.getLastError());
        return false;
    }

    return true;
}

bool QnOnvifPtzController::updatePreset(const QnPtzPreset &preset) {
    return createPreset(preset); // TODO: #Elric #PTZ wrong, update does not create new preset, and does not change saved position
}

bool QnOnvifPtzController::createPreset(const QnPtzPreset &preset) {
    QString ptzUrl = m_resource->getPtzUrl();
    if(ptzUrl.isEmpty())
        return false;

    QAuthenticator auth(m_resource->getAuth());
    PtzSoapWrapper ptz (ptzUrl.toStdString(), auth.user(), auth.password(), m_resource->getTimeDrift());

    SetPresetReq request;
    SetPresetResp response;
    request.ProfileToken = m_resource->getPtzProfileToken().toStdString();
    std::string stdPresetName = toLatinStdString(preset.name);
    request.PresetName = &stdPresetName;

    if (ptz.setPreset(request, response) != SOAP_OK) {
        qnWarning("Execution of PTZ create preset command for resource '%1' has failed with error %2.", m_resource->getName(), ptz.getLastError());
        return false;
    }

    QString token = QString::fromStdString(response.PresetToken);
    m_presetTokenById.insert(preset.id, token);
    m_presetNameByToken.insert(token, preset.name);
    return true;
}

#endif // ENABLE_ONVIF
