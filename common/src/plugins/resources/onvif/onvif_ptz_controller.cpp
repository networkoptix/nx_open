
#ifdef ENABLE_ONVIF

#include "onvif_ptz_controller.h"
#include "plugins/resources/onvif/onvif_resource.h"
#include "soap_wrapper.h"
#include "onvif/soapDeviceBindingProxy.h"
#include "common/common_module.h"


// -------------------------------------------------------------------------- //
// QnOnvifPtzController
// -------------------------------------------------------------------------- //
QnOnvifPtzController::QnOnvifPtzController(const QnPlOnvifResourcePtr &resource): 
    QnAbstractPtzController(resource),
    m_resource(resource),
    m_ptzCapabilities(0)
{
    m_ptzCapabilities = Qn::ContinuousPtzCapabilities | Qn::AbsolutePtzCapabilities;

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


    // TODO: #Elric make configurable
#if 0
    QString model = m_resource->getModel();
    if(model == lit("FW3471-PS-E")) {
        m_ptzCapabilities |= Qn::OctagonalPtzCapability;
        m_ptzCapabilities &= ~Qn::AbsolutePtzCapabilities;
    }
    if(model == lit("IPC-HDB3200C")) {
        m_ptzCapabilities = Qn::NoPtzCapabilities;
    }
    if(model == lit("DWC-MPTZ20X")) {
        m_ptzCapabilities |= Qn::OctagonalPtzCapability;
    }
    if(model == lit("DM368") || model == lit("FD8161") || model == lit("FD8362E") || model == lit("FD8361") || model == lit("FD8136") || model == lit("FD8162") || model == lit("FD8372") || model == lit("FD8135H") || model == lit("IP8151") || model == lit("IP8335H") || model == lit("IP8362") || model == lit("MD8562")) {
        m_ptzCapabilities = Qn::NoPtzCapabilities;
    }
#endif // TODO: #PTZ


    //qCritical() << "reading PTZ token finished. minX=" << m_xNativeVelocityCoeff.second;
}

Qn::PtzCapabilities QnOnvifPtzController::getCapabilities() 
{
    return m_ptzCapabilities;
}

int QnOnvifPtzController::stopMoveInternal()
{
    // TODO: #Elric TOTALLY EVIL!!! Refactor properly.
    QString model = m_resource->getModel();
    if(model == lit("SD8362") || model == lit("SD83X3") || model == lit("SD81X1"))
        return continuousMove(QVector3D());

    QAuthenticator auth(m_resource->getAuth());
    PtzSoapWrapper ptz (m_resource->getPtzfUrl().toStdString(), auth.user(), auth.password(), m_resource->getTimeDrift());
    _onvifPtz__Stop request;
    _onvifPtz__StopResponse response;
    request.ProfileToken = m_resource->getPtzProfileToken().toStdString();
    bool stopValue = true;
    request.PanTilt = &stopValue;
    request.Zoom = &stopValue;

    int rez = ptz.doStop(request, response);
    if (rez != SOAP_OK) {
        qCritical() << "Error executing PTZ stop command for resource " << m_resource->getUniqueId() << ". Error: " << ptz.getLastError();
    }
    return rez;
}

double QnOnvifPtzController::normalizeSpeed(qreal inputVelocity, const QPair<qreal, qreal>& nativeCoeff, qreal userCoeff)
{
    inputVelocity *= inputVelocity >= 0 ? nativeCoeff.first : -nativeCoeff.second;
    inputVelocity *= userCoeff;
    double rez = qBound(nativeCoeff.second, inputVelocity, nativeCoeff.first);
    return rez;
}

int QnOnvifPtzController::continuousMove(const QVector3D &speed)
{
    if(qFuzzyIsNull(speed))
        return stopMoveInternal();

    QVector3D localSpeed = speed;

    /*if(m_horizontalFlipped)
        localSpeed.setX(-localSpeed.x());
    if(m_verticalFlipped)
        localSpeed.setY(-localSpeed.y());*/

    QAuthenticator auth(m_resource->getAuth());
    PtzSoapWrapper ptz (m_resource->getPtzfUrl().toStdString().c_str(), auth.user(), auth.password(), m_resource->getTimeDrift());
    _onvifPtz__ContinuousMove request;
    _onvifPtz__ContinuousMoveResponse response;
    request.ProfileToken = m_resource->getPtzProfileToken().toStdString();
    request.Velocity = new onvifXsd__PTZSpeed();
    request.Velocity->PanTilt = new onvifXsd__Vector2D();
    request.Velocity->Zoom = new onvifXsd__Vector1D();

    request.Velocity->PanTilt->x = normalizeSpeed(speed.x(), m_xNativeVelocityCoeff, 1.0);
    request.Velocity->PanTilt->y = normalizeSpeed(speed.y(), m_yNativeVelocityCoeff, 1.0);
    request.Velocity->Zoom->x = normalizeSpeed(speed.z(), m_zoomNativeVelocityCoeff, 1.0);

    int rez = ptz.doContinuousMove(request, response);
    if (rez != SOAP_OK)
    {
        qCritical() << "Error executing PTZ move command for resource " << m_resource->getUniqueId() << ". Error: " << ptz.getLastError();
    }

    delete request.Velocity->Zoom;
    delete request.Velocity->PanTilt;
    delete request.Velocity;

    return rez;
}

int QnOnvifPtzController::absoluteMove(const QVector3D &position)
{
    QAuthenticator auth(m_resource->getAuth());
    PtzSoapWrapper ptz (m_resource->getPtzfUrl().toStdString().c_str(), auth.user(), auth.password(), m_resource->getTimeDrift());
    _onvifPtz__AbsoluteMove request;
    _onvifPtz__AbsoluteMoveResponse response;
    
    request.ProfileToken = m_resource->getPtzProfileToken().toStdString();

    request.Position = new onvifXsd__PTZVector();
    request.Position->PanTilt = new onvifXsd__Vector2D();
    request.Position->Zoom = new onvifXsd__Vector1D();
    request.Position->PanTilt->x = position.x();
    request.Position->PanTilt->y = position.y();
    request.Position->Zoom->x = position.z();

    request.Speed = new onvifXsd__PTZSpeed();
    request.Speed->PanTilt = new onvifXsd__Vector2D();
    request.Speed->Zoom = new onvifXsd__Vector1D();
    request.Speed->PanTilt->x = 1.0;
    request.Speed->PanTilt->y = 1.0;
    request.Speed->Zoom->x = 1.0;

    int rez = ptz.doAbsoluteMove(request, response);
    if (rez != SOAP_OK)
    {
        qCritical() << "Error executing PTZ absolute move command for resource " << m_resource->getUniqueId() << ". Error: " << ptz.getLastError();
    }

    delete request.Speed->Zoom;
    delete request.Speed->PanTilt;
    delete request.Speed;

    delete request.Position->Zoom;
    delete request.Position->PanTilt;
    delete request.Position;

    return rez;
}

int QnOnvifPtzController::getPosition(QVector3D *position)
{
    QAuthenticator auth(m_resource->getAuth());
    PtzSoapWrapper ptz (m_resource->getPtzfUrl().toStdString().c_str(), auth.user(), auth.password(), m_resource->getTimeDrift());
    _onvifPtz__GetStatus request;
    _onvifPtz__GetStatusResponse response;

    request.ProfileToken = m_resource->getPtzProfileToken().toStdString();

    *position = QVector3D();

    int rez = ptz.doGetStatus(request, response);
    if (rez != SOAP_OK)
    {
        qCritical() << "Error executing PTZ move command for resource " << m_resource->getUniqueId() << ". Error: " << ptz.getLastError();
    }
    else {
        if (response.PTZStatus && response.PTZStatus->Position && response.PTZStatus->Position->PanTilt)
        {
            position->setX(response.PTZStatus->Position->PanTilt->x);
            position->setY(response.PTZStatus->Position->PanTilt->y);
        }
        if (response.PTZStatus && response.PTZStatus->Position && response.PTZStatus->Position->Zoom)
            position->setZ(response.PTZStatus->Position->Zoom->x);
    }

    return rez;
}

int QnOnvifPtzController::getLimits(QnPtzLimits *limits) {
    return 1;
}

int QnOnvifPtzController::getFlip(Qt::Orientations *flip) {
    return 1;
}

int QnOnvifPtzController::relativeMove(qreal aspectRatio, const QRectF &viewport) {
    return 1;
}


#endif //ENABLE_ONVIF
