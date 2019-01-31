#ifdef ENABLE_ONVIF

#include "onvif_ptz_controller.h"

#include <onvif/soapDeviceBindingProxy.h>

#include <common/common_module.h>
#include <core/resource/resource_data.h>
#include <core/resource_management/resource_data_pool.h>
#include <onvif/soapImagingBindingProxy.h>
#include <onvif/soapPTZBindingProxy.h>
#include <plugins/resource/onvif/onvif_resource.h>
#include <nx/utils/math/fuzzy.h>
#include <nx/utils/scope_guard.h>
#include <nx/utils/log/log.h>
#include <nx/utils/log/assert.h>

#include "soap_wrapper.h"
#include <nx/utils/log/log_main.h>

using namespace nx::core;

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
    base_type(resource), m_resource(resource)
{
    m_limits.minPan = -1.0;
    m_limits.maxPan = 1.0;
    m_limits.minTilt = -1.0;
    m_limits.maxTilt = 1.0;
    m_limits.minFov = 0.0;
    m_limits.maxFov = 1.0;

    SpeedLimits defaultLimits(-QnAbstractPtzController::MaxPtzSpeed, QnAbstractPtzController::MaxPtzSpeed);
    m_panSpeedLimits = m_tiltSpeedLimits = m_zoomSpeedLimits = m_focusSpeedLimits = defaultLimits;

    const QnResourceData data = resource->resourceData();
    m_stopBroken = data.value<bool>(lit("onvifPtzStopBroken"), false);
    m_speedBroken = data.value<bool>(lit("onvifPtzSpeedBroken"), false);
    bool absoluteMoveBroken = data.value<bool>(lit("onvifPtzAbsoluteMoveBroken"), false);
    bool focusEnabled = data.value<bool>(lit("onvifPtzFocusEnabled"), false);
    bool nativePresetsAreEnabled = data.value<bool>(lit("onvifPtzPresetsEnabled"), false);

    const int digitsAfterDecimalPoint  = data.value<int>(lit("onvifPtzDigitsAfterDecimalPoint"), 10);
    sprintf( m_floatFormat, "%%.%df", digitsAfterDecimalPoint );
    sprintf( m_doubleFormat, "%%.%dlf", digitsAfterDecimalPoint );

    const QString ptzUrl = m_resource->getPtzUrl();
    if(ptzUrl.isEmpty())
        return;

    m_capabilities = data.value<Ptz::Capabilities>(ResourceDataKey::kOperationalPtzCapabilities);

    m_capabilities |= initMove();
    if(absoluteMoveBroken)
        m_capabilities &= ~(Ptz::AbsolutePtzCapabilities | Ptz::DevicePositioningPtzCapability);
    if(focusEnabled)
        m_capabilities |= initContinuousFocus();
    if(nativePresetsAreEnabled && !m_resource->getPtzUrl().isEmpty())
        m_capabilities |= (Ptz::PresetsPtzCapability | Ptz::NativePresetsPtzCapability);

    m_resource->setDefaultPreferredPtzPresetType(
        nativePresetsAreEnabled ? ptz::PresetType::native : ptz::PresetType::system);

    // TODO: #PTZ #Elric actually implement flip!
}

Ptz::Capabilities QnOnvifPtzController::initMove()
{
    PtzSoapWrapper ptz(m_resource);
    ptz.soap()->float_format = m_floatFormat;
    ptz.soap()->double_format = m_doubleFormat;

    _onvifPtz__GetConfigurations request;
    _onvifPtz__GetConfigurationsResponse response;
    if (ptz.doGetConfigurations(request, response) != SOAP_OK)
        return Ptz::NoPtzCapabilities;
    if(response.PTZConfiguration.empty() || !response.PTZConfiguration[0])
        return Ptz::NoPtzCapabilities;

    // TODO: #PTZ #Elric we can init caps by examining spaces in response!

    Ptz::Capabilities configCapabilities;
    if (response.PTZConfiguration[0]->DefaultContinuousPanTiltVelocitySpace)
        configCapabilities |= Ptz::ContinuousPanCapability | Ptz::ContinuousTiltCapability;
    if (response.PTZConfiguration[0]->DefaultContinuousZoomVelocitySpace)
        configCapabilities |= Ptz::ContinuousZoomCapability;
    if (response.PTZConfiguration[0]->DefaultAbsolutePantTiltPositionSpace)
        configCapabilities |= Ptz::AbsolutePanCapability | Ptz::AbsoluteTiltCapability;
    if (response.PTZConfiguration[0]->DefaultAbsoluteZoomPositionSpace)
        configCapabilities |= Ptz::AbsoluteZoomCapability;
    if (response.PTZConfiguration[0]->DefaultRelativePanTiltTranslationSpace)
        configCapabilities |= Ptz::RelativePanTiltCapabilities;
    if (response.PTZConfiguration[0]->DefaultRelativeZoomTranslationSpace)
        configCapabilities |= Ptz::RelativeZoomCapability;

    _onvifPtz__GetNode nodeRequest;
    _onvifPtz__GetNodeResponse nodeResponse;
    nodeRequest.NodeToken = response.PTZConfiguration[0]->NodeToken;
    if (ptz.doGetNode(nodeRequest, nodeResponse) != SOAP_OK)
        return Ptz::NoPtzCapabilities;

    if (!nodeResponse.PTZNode || !nodeResponse.PTZNode->SupportedPTZSpaces)
        return Ptz::NoPtzCapabilities;
    onvifXsd__PTZSpaces *spaces = nodeResponse.PTZNode->SupportedPTZSpaces;

    Ptz::Capabilities nodeCapabilities = Ptz::NoPtzCapabilities;
    if(!spaces->ContinuousPanTiltVelocitySpace.empty() && spaces->ContinuousPanTiltVelocitySpace[0]) {
        if(spaces->ContinuousPanTiltVelocitySpace[0]->XRange) {
            m_panSpeedLimits.min = spaces->ContinuousPanTiltVelocitySpace[0]->XRange->Min;
            m_panSpeedLimits.max = spaces->ContinuousPanTiltVelocitySpace[0]->XRange->Max;
            nodeCapabilities |= Ptz::ContinuousPanCapability;
        }
        if(spaces->ContinuousPanTiltVelocitySpace[0]->YRange) {
            m_tiltSpeedLimits.min = spaces->ContinuousPanTiltVelocitySpace[0]->YRange->Min;
            m_tiltSpeedLimits.max = spaces->ContinuousPanTiltVelocitySpace[0]->YRange->Max;
            nodeCapabilities |= Ptz::ContinuousTiltCapability;
        }
    }
    if(!spaces->ContinuousZoomVelocitySpace.empty() && spaces->ContinuousZoomVelocitySpace[0]) {
        if(spaces->ContinuousZoomVelocitySpace[0]->XRange) {
            m_zoomSpeedLimits.min = spaces->ContinuousZoomVelocitySpace[0]->XRange->Min;
            m_zoomSpeedLimits.max = spaces->ContinuousZoomVelocitySpace[0]->XRange->Max;
            nodeCapabilities |= Ptz::ContinuousZoomCapability;
        }
    }
    if(!spaces->AbsolutePanTiltPositionSpace.empty() && spaces->AbsolutePanTiltPositionSpace[0]) {
        if(spaces->AbsolutePanTiltPositionSpace[0]->XRange) {
            m_limits.minPan = spaces->AbsolutePanTiltPositionSpace[0]->XRange->Min;
            m_limits.maxPan = spaces->AbsolutePanTiltPositionSpace[0]->XRange->Max;
            nodeCapabilities |= Ptz::AbsolutePanCapability;
        }
        if(spaces->AbsolutePanTiltPositionSpace[0]->YRange) {
            m_limits.minTilt = spaces->AbsolutePanTiltPositionSpace[0]->YRange->Min;
            m_limits.maxTilt = spaces->AbsolutePanTiltPositionSpace[0]->YRange->Max;
            nodeCapabilities |= Ptz::AbsoluteTiltCapability;
        }
    }
    if(!spaces->AbsoluteZoomPositionSpace.empty() && spaces->AbsoluteZoomPositionSpace[0]) {
        if(spaces->AbsoluteZoomPositionSpace[0]->XRange) {
            m_limits.minFov = spaces->AbsoluteZoomPositionSpace[0]->XRange->Min;
            m_limits.maxFov = spaces->AbsoluteZoomPositionSpace[0]->XRange->Max;
            nodeCapabilities |= Ptz::AbsoluteZoomCapability;
        }
    }

    if (!spaces->RelativePanTiltTranslationSpace.empty()
        && spaces->RelativePanTiltTranslationSpace[0])
    {
        if (spaces->RelativePanTiltTranslationSpace[0]->XRange)
            nodeCapabilities |= Ptz::RelativePanTiltCapabilities;
    }

    if (!spaces->RelativeZoomTranslationSpace.empty() && spaces->RelativeZoomTranslationSpace[0])
    {
        if (spaces->RelativeZoomTranslationSpace[0]->XRange)
            nodeCapabilities |= Ptz::RelativeZoomCapability;
    }

    Ptz::Capabilities result = configCapabilities & nodeCapabilities;

    if (nodeResponse.PTZNode->MaximumNumberOfPresets > 0)
        result |= (Ptz::PresetsPtzCapability | Ptz::NativePresetsPtzCapability);

    if(result & Ptz::AbsolutePtzCapabilities) {
        result |= Ptz::DevicePositioningPtzCapability;

        if(nodeCapabilities & Ptz::AbsolutePtzCapabilities)
            result |= Ptz::LimitsPtzCapability;
    }

    return result;
}

bool QnOnvifPtzController::readBuiltinPresets()
{
    if (m_ptzPresetsIsRead)
        return true;

    PtzSoapWrapper ptz(m_resource);
    if (!ptz)
    {
        // #TODO: log.
        return false;
    }
    ptz.soap()->float_format = m_floatFormat;
    ptz.soap()->double_format = m_doubleFormat;

    GetPresetsReq request;
    request.ProfileToken = m_resource->ptzProfileToken();
    GetPresetsResp response;
    if (ptz.getPresets(request, response) != SOAP_OK)
    {
        // #TODO: log.
        return false;
    }
    m_presetNameByToken.clear();
    for(onvifXsd__PTZPreset* preset: response.Preset)
    {
        if (!preset || !preset->token)
        {
            // #TODO: log.
            return false;
        }
        QString id = QString::fromStdString(*preset->token);
        QString name = lit("Preset %1").arg(id);

        if (preset->Name)
            name = fromLatinStdString(*preset->Name);

        m_presetNameByToken.insert(id, name);
    }

    m_ptzPresetsIsRead = true;
    return true;
}

Ptz::Capabilities QnOnvifPtzController::initContinuousFocus() {
    QString imagingUrl = m_resource->getImagingUrl();
    if(imagingUrl.isEmpty())
        return Ptz::NoPtzCapabilities;

    QAuthenticator auth = m_resource->getAuth();
    ImagingSoapWrapper imaging(
        m_resource->onvifTimeouts(),
        imagingUrl.toStdString(), auth.user(), auth.password(), m_resource->getTimeDrift());
    imaging.soap()->float_format = m_floatFormat;
    imaging.soap()->double_format = m_doubleFormat;

    _onvifImg__GetMoveOptions moveOptionsRequest;
    moveOptionsRequest.VideoSourceToken = m_resource->videoSourceToken();

    _onvifImg__GetMoveOptionsResponse moveOptionsResponse;
    if (imaging.getMoveOptions(moveOptionsRequest, moveOptionsResponse) != SOAP_OK)
        return Ptz::NoPtzCapabilities;

    if(!moveOptionsResponse.MoveOptions || !moveOptionsResponse.MoveOptions->Continuous || !moveOptionsResponse.MoveOptions->Continuous->Speed)
        return Ptz::NoPtzCapabilities;
    onvifXsd__FloatRange *speedLimits = moveOptionsResponse.MoveOptions->Continuous->Speed;

    m_focusSpeedLimits.min = speedLimits->Min;
    m_focusSpeedLimits.max = speedLimits->Max;
    return Ptz::ContinuousFocusCapability;
}

double QnOnvifPtzController::normalizeSpeed(qreal speed, const SpeedLimits &speedLimits) {
    speed *= speed >= 0 ? speedLimits.max : -speedLimits.min;
    return qBound(speedLimits.min, speed, speedLimits.max);
}

Ptz::Capabilities QnOnvifPtzController::getCapabilities(const nx::core::ptz::Options& options) const
{
    if (options.type != ptz::Type::operational)
        return Ptz::NoPtzCapabilities;

    return m_capabilities;
}

bool QnOnvifPtzController::stopInternal()
{
    PtzSoapWrapper ptz(m_resource);
    if (!ptz)
    {
        // #TODO: LOG.
        return false;
    }
    ptz.soap()->float_format = m_floatFormat;
    ptz.soap()->double_format = m_doubleFormat;

    bool stopValue = true;

    _onvifPtz__Stop request;
    request.ProfileToken = m_resource->ptzProfileToken();
    request.PanTilt = &stopValue;
    request.Zoom = &stopValue;

    _onvifPtz__StopResponse response;
    if (ptz.doStop(request, response) != SOAP_OK) {
        qnWarning("Execution of PTZ stop command for resource '%1' has failed with error %2.", m_resource->getName(), ptz.getLastErrorDescription());
        return false;
    }

    return true;
}

bool QnOnvifPtzController::moveInternal(const nx::core::ptz::Vector& speedVector)
{
    PtzSoapWrapper ptz(m_resource);
    if (!ptz)
    {
        NX_WARNING(
            this,
            lm("Can't execute PTZ moveInternal for resource '%1' because of no PTZ url.").arg(m_resource->getName()));
        return false;
    }

    ptz.soap()->float_format = m_floatFormat;
    ptz.soap()->double_format = m_doubleFormat;

    onvifXsd__Vector2D onvifPanTiltSpeed;
    onvifPanTiltSpeed.x = normalizeSpeed(speedVector.pan, m_panSpeedLimits);
    onvifPanTiltSpeed.y = normalizeSpeed(speedVector.tilt, m_tiltSpeedLimits);

    onvifXsd__Vector1D onvifZoomSpeed;
    onvifZoomSpeed.x = normalizeSpeed(speedVector.zoom, m_zoomSpeedLimits);

    onvifXsd__PTZSpeed onvifSpeed;
    onvifSpeed.PanTilt = &onvifPanTiltSpeed;
    onvifSpeed.Zoom = &onvifZoomSpeed;

    _onvifPtz__ContinuousMove request;
    request.ProfileToken = m_resource->ptzProfileToken();
    request.Velocity = &onvifSpeed;

    _onvifPtz__ContinuousMoveResponse response;
    if (ptz.doContinuousMove(request, response) != SOAP_OK) {
        qnWarning("Execution of PTZ continuous move command for resource '%1' has failed with error %2.", m_resource->getName(), ptz.getLastErrorDescription());
        return false;
    }

    return true;
}

bool QnOnvifPtzController::continuousMove(
    const nx::core::ptz::Vector& speed,
    const nx::core::ptz::Options& options)
{
    if (options.type != ptz::Type::operational)
    {
        NX_WARNING(
            this,
            lm("Continuous movement - wrong PTZ type. "
                "Only operational PTZ is supported. Resource %1 (%2)")
                .args(resource()->getName(), resource()->getId()));

        return false;
    }

    if(qFuzzyIsNull(speed) && !m_stopBroken) {
        return stopInternal();
    } else {
        return moveInternal(speed);
    }
}

bool QnOnvifPtzController::continuousFocus(
    qreal speed,
    const nx::core::ptz::Options& options)
{
    if (options.type != ptz::Type::operational)
    {
        NX_WARNING(
            this,
            lm("Continuous focus - wrong PTZ type. "
                "Only operational PTZ is supported. Resource %1 (%2)")
                .args(resource()->getName(), resource()->getId()));

        return false;
    }

    QString imagingUrl = m_resource->getImagingUrl();
    if(imagingUrl.isEmpty())
        return false;

    QAuthenticator auth = m_resource->getAuth();
    ImagingSoapWrapper imaging(
        m_resource->onvifTimeouts(),
        imagingUrl.toStdString(), auth.user(), auth.password(), m_resource->getTimeDrift());
    imaging.soap()->float_format = m_floatFormat;
    imaging.soap()->double_format = m_doubleFormat;

    onvifXsd__ContinuousFocus onvifContinuousFocus;
    onvifContinuousFocus.Speed = normalizeSpeed(speed, m_focusSpeedLimits);

    onvifXsd__FocusMove onvifFocus;
    onvifFocus.Continuous = &onvifContinuousFocus;

    _onvifImg__Move request;
    request.VideoSourceToken = m_resource->videoSourceToken();
    request.Focus = &onvifFocus;

    _onvifImg__MoveResponse response;
    if (imaging.move(request, response) != SOAP_OK) {
        qnWarning("Execution of PTZ continuous focus command for resource '%1' has failed with error %2.", m_resource->getName(), imaging.getLastErrorDescription());
        return false;
    }

    return true;
}

bool QnOnvifPtzController::absoluteMove(
    Qn::PtzCoordinateSpace space,
    const nx::core::ptz::Vector& position,
    qreal speed,
    const nx::core::ptz::Options& options)
{
    if (options.type != ptz::Type::operational)
    {
        NX_WARNING(
            this,
            lm("Absolute movement - wrong PTZ type. "
                "Only operational PTZ is supported. Resource %1 (%2)")
                .args(resource()->getName(), resource()->getId()));

        return false;
    }

    if(space != Qn::DevicePtzCoordinateSpace)
        return false;

    PtzSoapWrapper ptz(m_resource);
    if (!ptz)
    {
        // #TODO: log
        return false;
    }
    ptz.soap()->float_format = m_floatFormat;
    ptz.soap()->double_format = m_doubleFormat;

    onvifXsd__Vector2D onvifPanTilt;
    onvifPanTilt.x = position.pan;
    onvifPanTilt.y = position.tilt;

    onvifXsd__Vector1D onvifZoom;
    onvifZoom.x = position.zoom;

    onvifXsd__PTZVector onvifPosition;
    onvifPosition.PanTilt = &onvifPanTilt;
    onvifPosition.Zoom = &onvifZoom;

    _onvifPtz__AbsoluteMove request;
    request.ProfileToken = m_resource->ptzProfileToken();
    request.Position = &onvifPosition;

    onvifXsd__Vector2D onvifPanTiltSpeed;
    onvifXsd__Vector1D onvifZoomSpeed;
    onvifXsd__PTZSpeed onvifSpeed;
    if (!m_speedBroken)
    {
        // TODO: #Elric #PTZ do we need to adjust speed to speed limits here?
        onvifPanTiltSpeed.x = speed;
        onvifPanTiltSpeed.y = speed;

        onvifZoomSpeed.x = speed;

        onvifSpeed.PanTilt = &onvifPanTiltSpeed;
        onvifSpeed.Zoom = &onvifZoomSpeed;

        request.Speed = &onvifSpeed;
    }

    _onvifPtz__AbsoluteMoveResponse response;

    const bool result = ptz.doAbsoluteMove(request, response) == SOAP_OK;
    if (!result)
        qnWarning("Execution of PTZ absolute move command for resource '%1' has failed with error %2.", m_resource->getName(), ptz.getLastErrorDescription());

    return result;
}

bool QnOnvifPtzController::relativeMove(
    const nx::core::ptz::Vector& relativeMovementVector,
    const nx::core::ptz::Options& options)
{
    if (options.type != ptz::Type::operational)
    {
        NX_WARNING(
            this,
            lm("Relative movement - wrong PTZ type. "
                "Only operational PTZ is supported. Resource %1 (%2)")
                .args(resource()->getName(), resource()->getId()));

        return false;
    }

    PtzSoapWrapper ptz(m_resource);
    if (!ptz)
    {
        // #TODO: log.
        return false;
    }
    ptz.soap()->float_format = m_floatFormat;
    ptz.soap()->double_format = m_doubleFormat;

    onvifXsd__Vector2D panTilt;
    panTilt.x = relativeMovementVector.pan;
    panTilt.y = relativeMovementVector.tilt;

    onvifXsd__Vector1D zoom;
    zoom.x = relativeMovementVector.zoom;

    onvifXsd__PTZVector translation;
    translation.PanTilt = &panTilt;
    translation.Zoom = &zoom;

    RelativeMoveReq request;
    request.ProfileToken = m_resource->ptzProfileToken();
    request.Speed = nullptr; //< Always use the default speed.
    request.Translation = &translation;

    RelativeMoveResp response;
    if (!ptz.doRelativeMove(request, response))
    {
        NX_ERROR(this, lm("Failed to perform relative movement. Resource %1 (%2), error: %3")
            .args(m_resource->getName(), m_resource->getId(), ptz.getLastErrorDescription()));

        return false;
    }

    return true;
}

bool QnOnvifPtzController::getPosition(
    Qn::PtzCoordinateSpace space,
    nx::core::ptz::Vector* outPosition,
    const nx::core::ptz::Options& options) const
{
    if (options.type != ptz::Type::operational)
    {
        NX_WARNING(this,
            lm("Getting current position. "
                "Wrong PTZ type. Only operational PTZ is supported. Resource %1 (%2)")
                .args(resource()->getName(), resource()->getId()));
        return false;
    }

    if (space != Qn::DevicePtzCoordinateSpace)
    {
        NX_WARNING(this,
            lm("Getting current position. "
                "Wrong PTZ type. Only DevicePtzCoordinateSpace is supported. Resource %1 (%2)")
            .args(resource()->getName(), resource()->getId()));
        return false;
    }

    PtzSoapWrapper ptz(m_resource);
    if (!ptz)
    {
        NX_WARNING(this,
            lm("Getting current position. "
                "PtzUrl is empty. Resource %1 (%2)")
            .args(m_resource->getName(), m_resource->getId()));
        return false;
    }
    ptz.soap()->float_format = m_floatFormat;
    ptz.soap()->double_format = m_doubleFormat;

    _onvifPtz__GetStatus request;
    request.ProfileToken = m_resource->ptzProfileToken();

    _onvifPtz__GetStatusResponse response;
    if (ptz.doGetStatus(request, response) != SOAP_OK) {
        NX_WARNING(this, lm("Execution of PTZ status command for resource '%1' has failed with error %2.")
            .args(m_resource->getName(), ptz.getLastErrorDescription()));
        return false;
    }

    *outPosition = nx::core::ptz::Vector();

    if (response.PTZStatus && response.PTZStatus->Position)
    {
        if(response.PTZStatus->Position->PanTilt)
        {
            outPosition->pan = response.PTZStatus->Position->PanTilt->x;
            outPosition->tilt = response.PTZStatus->Position->PanTilt->y;
            if ((outPosition->pan == 0.0) && (outPosition->tilt == 0.0))
            {
                NX_WARNING(this,
                    lm("Getting current position. "
                        "Zero values of Pan and Tilt are received. Resource %1 (%2).")
                    .args(resource()->getName(), resource()->getId()));
            }
        }
        else
        {
            NX_WARNING(this,
                lm("Getting current position. "
                    "Pan and Tilt are absent in response. Resource %1 (%2).")
                .args(resource()->getName(), resource()->getId()));
        }
        if(response.PTZStatus->Position->Zoom)
        {
            outPosition->zoom = response.PTZStatus->Position->Zoom->x;
            if (outPosition->zoom == 0.0)
            {
                NX_WARNING(this,
                    lm("Getting current position. "
                        "Zero value of Zoom is received. Resource %1 (%2).")
                    .args(resource()->getName(), resource()->getId()));
            }
        }
        else
        {
            NX_WARNING(this,
                lm("Getting current position. "
                    "Zoom is absent in response. Resource %1 (%2).")
                .args(resource()->getName(), resource()->getId()));
        }
    }
    return true;
}

bool QnOnvifPtzController::getLimits(
    Qn::PtzCoordinateSpace space,
    QnPtzLimits *limits,
    const nx::core::ptz::Options& options) const
{
    if (options.type != ptz::Type::operational)
    {
        NX_WARNING(
            this,
            lm("Getting limits - wrong PTZ type. "
                "Only operational PTZ is supported. Resource %1 (%2)")
                .args(resource()->getName(), resource()->getId()));

        return false;
    }

    if(space != Qn::DevicePtzCoordinateSpace)
        return false;

    *limits = m_limits;
    return true;
}

bool QnOnvifPtzController::getFlip(
    Qt::Orientations *flip,
    const nx::core::ptz::Options& options) const
{
    if (options.type != ptz::Type::operational)
    {
        NX_WARNING(
            this,
            lm("Getting flip - wrong PTZ type. "
                "Only operational PTZ is supported. Resource %1 (%2)")
                .args(resource()->getName(), resource()->getId()));

        return false;
    }

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
    readBuiltinPresets();
    QString internalId = m_presetTokenById.value(presetId);
    if (internalId.isEmpty())
        internalId = presetId;
    return m_presetNameByToken.value(internalId);
}

bool QnOnvifPtzController::removePreset(const QString &presetId)
{
    QnMutexLocker lk( &m_mutex );

    QAuthenticator auth = m_resource->getAuth();
    PtzSoapWrapper ptz(m_resource);
    if (!ptz)
    {
        // #TODO: log.
        return false;
    }
    ptz.soap()->float_format = m_floatFormat;
    ptz.soap()->double_format = m_doubleFormat;

    RemovePresetReq request;
    RemovePresetResp response;
    request.ProfileToken = m_resource->ptzProfileToken();
    request.PresetToken = presetToken(presetId).toStdString();
    if (ptz.removePreset(request, response) != SOAP_OK) {
        qnWarning("Execution of PTZ remove preset command for resource '%1' has failed with error %2.", m_resource->getName(), ptz.getLastErrorDescription());
        return false;
    }

    return true;
}

bool QnOnvifPtzController::getPresets(QnPtzPresetList *presets) const
{
    QnMutexLocker lk( &m_mutex );

    auto nonConstThis = const_cast<QnOnvifPtzController*>(this);
    if (!nonConstThis->readBuiltinPresets())
        return false;
    for (auto itr = m_presetNameByToken.begin(); itr != m_presetNameByToken.end(); ++itr)
        presets->push_back(QnPtzPreset(itr.key(), itr.value()));
    return true;
}

bool QnOnvifPtzController::activatePreset(const QString &presetId, qreal speed)
{
    QnMutexLocker lk( &m_mutex );

    PtzSoapWrapper ptz(m_resource);
    if (!ptz)
    {
        // #TODO: log.
        return false;
    }
    ptz.soap()->float_format = m_floatFormat;
    ptz.soap()->double_format = m_doubleFormat;

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
    request.ProfileToken = m_resource->ptzProfileToken();
    request.PresetToken = presetToken(presetId).toStdString();
    if (!m_speedBroken)
    {
        // TODO: #Elric #PTZ do we need to adjust speed to speed limits here?
        onvifXsd__Vector2D onvifPanTiltSpeed;
        onvifPanTiltSpeed.x = speed;
        onvifPanTiltSpeed.y = speed;

        onvifXsd__Vector1D onvifZoomSpeed;
        onvifZoomSpeed.x = speed;

        onvifXsd__PTZSpeed onvifSpeed;
        onvifSpeed.PanTilt = &onvifPanTiltSpeed;
        onvifSpeed.Zoom = &onvifZoomSpeed;

        request.Speed = &onvifSpeed;
    }

    if (ptz.gotoPreset(request, response) != SOAP_OK) {
        qnWarning("Execution of PTZ goto preset command for resource '%1' has failed with error %2.", m_resource->getName(), ptz.getLastErrorDescription());
        return false;
    }

    return true;
}

bool QnOnvifPtzController::updatePreset(const QnPtzPreset &preset)
{
    return createPreset(preset); // TODO: #Elric #PTZ wrong, update does not create new preset, and does not change saved position
}

bool QnOnvifPtzController::createPreset(const QnPtzPreset &preset)
{
    QnMutexLocker lk( &m_mutex );

    if (!readBuiltinPresets())
        return false;

    PtzSoapWrapper ptz(m_resource);
    if (!ptz)
    {
        // #TODO: log.
        return false;
    }
    ptz.soap()->float_format = m_floatFormat;
    ptz.soap()->double_format = m_doubleFormat;

    SetPresetReq request;
    SetPresetResp response;
    request.ProfileToken = m_resource->ptzProfileToken();
    std::string stdPresetName = toLatinStdString(preset.name);
    request.PresetName = &stdPresetName;

    if (ptz.setPreset(request, response) != SOAP_OK) {
        qnWarning("Execution of PTZ create preset command for resource '%1' has failed with error %2.", m_resource->getName(), ptz.getLastErrorDescription());
        return false;
    }

    QString token = QString::fromStdString(response.PresetToken);
    m_presetTokenById.insert(preset.id, token);
    m_presetNameByToken.insert(token, preset.name);
    return true;
}

#endif // ENABLE_ONVIF
