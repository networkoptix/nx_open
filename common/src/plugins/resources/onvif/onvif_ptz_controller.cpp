#ifdef ENABLE_ONVIF

#include "onvif_ptz_controller.h"

#include <onvif/soapDeviceBindingProxy.h>

#include <common/common_module.h>
#include <core/resource/resource_data.h>
#include <core/resource_managment/resource_data_pool.h>
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
    m_capabilities = Qn::ContinuousPtzCapabilities | Qn::AbsolutePtzCapabilities | Qn::DevicePositioningPtzCapability | Qn::LimitsPtzCapability | Qn::FlipPtzCapability;

    // TODO: #PTZ #Elric actually implement flip!

    QnResourceData resourceData = qnCommon->dataPool()->data(resource);
    m_stopBroken = resourceData.value<bool>(lit("onvifPtzStopBroken"), false);

    if(resourceData.value<bool>(lit("onvifPtzBroken"), false)) {
        m_capabilities = Qn::NoPtzCapabilities;
        return;
    }

    QAuthenticator auth(m_resource->getAuth());
    PtzSoapWrapper ptz (m_resource->getPtzfUrl().toStdString(), auth.user(), auth.password(), m_resource->getTimeDrift());

    _onvifPtz__GetConfigurations request;
    _onvifPtz__GetConfigurationsResponse response;
    if (ptz.doGetConfigurations(request, response) != SOAP_OK || response.PTZConfiguration.size() > 0) {
        //qCritical() << "!!!";
    }

    _onvifPtz__GetNode nodeRequest;
    _onvifPtz__GetNodeResponse nodeResponse;
    nodeRequest.NodeToken = response.PTZConfiguration[0]->NodeToken; //m_resource->getPtzConfigurationToken().toStdString(); // response.PTZConfiguration[0]->NodeToken;

    if (ptz.doGetNode(nodeRequest, nodeResponse) == SOAP_OK)
    {
        //qCritical() << "reading PTZ token success";
        if (nodeResponse.PTZNode) 
        {
            //qCritical() << "reading PTZ token success and data exists";

            onvifXsd__PTZNode* ptzNode = nodeResponse.PTZNode;
            //m_ptzToken = QString::fromStdString(ptzNode[0].token);
            if (ptzNode[0].SupportedPTZSpaces) 
            {
                onvifXsd__PTZSpaces* spaces = ptzNode[0].SupportedPTZSpaces;
                if (spaces->ContinuousPanTiltVelocitySpace.size() > 0 && spaces->ContinuousPanTiltVelocitySpace[0]) {
                    if (spaces->ContinuousPanTiltVelocitySpace[0]->XRange) {
                        m_xNativeVelocityCoeff.first = spaces->ContinuousPanTiltVelocitySpace[0]->XRange->Max;
                        m_xNativeVelocityCoeff.second = spaces->ContinuousPanTiltVelocitySpace[0]->XRange->Min;
                    }
                    if (spaces->ContinuousPanTiltVelocitySpace[0]->YRange) {
                        m_yNativeVelocityCoeff.first = spaces->ContinuousPanTiltVelocitySpace[0]->YRange->Max;
                        m_yNativeVelocityCoeff.second = spaces->ContinuousPanTiltVelocitySpace[0]->YRange->Min;
                    }
                }
                if (spaces->ContinuousZoomVelocitySpace.size() > 0 && spaces->ContinuousZoomVelocitySpace[0]) {
                    if (spaces->ContinuousZoomVelocitySpace[0]->XRange) {
                        m_zoomNativeVelocityCoeff.first = spaces->ContinuousZoomVelocitySpace[0]->XRange->Max;
                        m_zoomNativeVelocityCoeff.second = spaces->ContinuousZoomVelocitySpace[0]->XRange->Min;
                    }
                }
            }
        }
    }
    else {
        //qCritical() << "can't read PTZ node info. errCode=" << ptz.getLastError() << ". Use default ranges";
    }

    /* Sane onvif default. */
    m_limits.minPan = -1.0;
    m_limits.maxPan = 1.0;
    m_limits.minTilt = -1.0;
    m_limits.maxTilt = 1.0;
    m_limits.minFov = 0.0;
    m_limits.maxFov = 1.0;
}

Qn::PtzCapabilities QnOnvifPtzController::getCapabilities() {
    return m_capabilities;
}

double QnOnvifPtzController::normalizeSpeed(qreal inputVelocity, const QPair<qreal, qreal>& nativeCoeff, qreal userCoeff) {
    inputVelocity *= inputVelocity >= 0 ? nativeCoeff.first : -nativeCoeff.second;
    inputVelocity *= userCoeff;
    double rez = qBound(nativeCoeff.second, inputVelocity, nativeCoeff.first);
    return rez;
}

bool QnOnvifPtzController::stopInternal() {
    QAuthenticator auth(m_resource->getAuth());
    PtzSoapWrapper ptz (m_resource->getPtzfUrl().toStdString(), auth.user(), auth.password(), m_resource->getTimeDrift());

    bool stopValue = true;

    _onvifPtz__Stop request;
    request.ProfileToken = m_resource->getPtzProfileToken().toStdString();
    request.PanTilt = &stopValue;
    request.Zoom = &stopValue;

    _onvifPtz__StopResponse response;
    if (ptz.doStop(request, response) != SOAP_OK) {
        qCritical() << "Error executing PTZ stop command for resource " << m_resource->getUniqueId() << ". Error: " << ptz.getLastError();
        return false;
    }
    
    return true;
}

bool QnOnvifPtzController::moveInternal(const QVector3D &speed) {
    QAuthenticator auth(m_resource->getAuth());
    PtzSoapWrapper ptz (m_resource->getPtzfUrl().toStdString().c_str(), auth.user(), auth.password(), m_resource->getTimeDrift());

    QVector3D localSpeed = speed;

    onvifXsd__Vector2D onvifPanTiltSpeed;
    onvifPanTiltSpeed.x = normalizeSpeed(speed.x(), m_xNativeVelocityCoeff, 1.0);
    onvifPanTiltSpeed.y = normalizeSpeed(speed.y(), m_yNativeVelocityCoeff, 1.0);;

    onvifXsd__Vector1D onvifZoomSpeed;
    onvifZoomSpeed.x = normalizeSpeed(speed.z(), m_zoomNativeVelocityCoeff, 1.0);

    onvifXsd__PTZSpeed onvifSpeed;
    onvifSpeed.PanTilt = &onvifPanTiltSpeed;
    onvifSpeed.Zoom = &onvifZoomSpeed;

    _onvifPtz__ContinuousMove request;
    request.ProfileToken = m_resource->getPtzProfileToken().toStdString();
    request.Velocity = &onvifSpeed;

    _onvifPtz__ContinuousMoveResponse response;
    if (ptz.doContinuousMove(request, response) != SOAP_OK) {
        qCritical() << "Error executing PTZ move command for resource " << m_resource->getUniqueId() << ". Error: " << ptz.getLastError();
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

bool QnOnvifPtzController::absoluteMove(Qn::PtzCoordinateSpace space, const QVector3D &position) {
    if(space != Qn::DeviceCoordinateSpace)
        return false;

    QAuthenticator auth(m_resource->getAuth());
    PtzSoapWrapper ptz (m_resource->getPtzfUrl().toStdString().c_str(), auth.user(), auth.password(), m_resource->getTimeDrift());
    
    onvifXsd__Vector2D onvifPanTilt;
    onvifPanTilt.x = position.x();
    onvifPanTilt.y = position.y();

    onvifXsd__Vector1D onvifZoom;
    onvifZoom.x = position.z();

    onvifXsd__PTZVector onvifPosition;
    onvifPosition.PanTilt = &onvifPanTilt;
    onvifPosition.Zoom = &onvifZoom;

    onvifXsd__Vector2D onvifPanTiltSpeed;
    onvifPanTiltSpeed.x = 1.0;
    onvifPanTiltSpeed.y = 1.0;

    onvifXsd__Vector1D onvifZoomSpeed;
    onvifZoomSpeed.x = 1.0;

    onvifXsd__PTZSpeed onvifSpeed;
    onvifSpeed.PanTilt = &onvifPanTiltSpeed;
    onvifSpeed.Zoom = &onvifZoomSpeed;

    _onvifPtz__AbsoluteMove request;
    request.ProfileToken = m_resource->getPtzProfileToken().toStdString();
    request.Position = &onvifPosition;
    request.Speed = &onvifSpeed;

    _onvifPtz__AbsoluteMoveResponse response;
    if (ptz.doAbsoluteMove(request, response) != SOAP_OK) {
        qCritical() << "Error executing PTZ absolute move command for resource " << m_resource->getUniqueId() << ". Error: " << ptz.getLastError();
        return false;
    }

    return true;
}

bool QnOnvifPtzController::getPosition(Qn::PtzCoordinateSpace space, QVector3D *position) {
    if(space != Qn::DeviceCoordinateSpace)
        return false;

    QAuthenticator auth(m_resource->getAuth());
    PtzSoapWrapper ptz (m_resource->getPtzfUrl().toStdString().c_str(), auth.user(), auth.password(), m_resource->getTimeDrift());
    
    _onvifPtz__GetStatus request;
    request.ProfileToken = m_resource->getPtzProfileToken().toStdString();

    _onvifPtz__GetStatusResponse response;
    if (ptz.doGetStatus(request, response) != SOAP_OK) {
        qCritical() << "Error executing PTZ move command for resource " << m_resource->getUniqueId() << ". Error: " << ptz.getLastError();
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
    if(space != Qn::DeviceCoordinateSpace)
        return false;

    *limits = m_limits;
    return true;
}

bool QnOnvifPtzController::getFlip(Qt::Orientations *flip) {
    *flip = 0; // TODO: #PTZ #Elric
    return true;
}

#endif //ENABLE_ONVIF
