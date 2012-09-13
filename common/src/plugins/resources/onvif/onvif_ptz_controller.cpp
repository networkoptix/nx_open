#include "onvif_ptz_controller.h"
#include "plugins/resources/onvif/onvif_resource.h"
#include "soap_wrapper.h"
#include "onvif/soapDeviceBindingProxy.h"

QnOnvifPtzController::QnOnvifPtzController(QnResourcePtr res): QnAbstractPtzController(res)
{
    m_res = qSharedPointerDynamicCast<QnPlOnvifResource> (res);
    m_xNativeVelocityCoeff.first = 1.0;
    m_yNativeVelocityCoeff.first = 1.0;
    m_zoomNativeVelocityCoeff.first = 1.0;
    m_xNativeVelocityCoeff.second = -1.0;
    m_yNativeVelocityCoeff.second = -1.0;
    m_zoomNativeVelocityCoeff.second = -1.0;

    // read PTZ params

    if (m_res->getPtzfUrl().isEmpty())
        return;

    QAuthenticator auth(m_res->getAuth());
    PtzSoapWrapper ptz (m_res->getPtzfUrl().toStdString().c_str(), auth.user().toStdString(), auth.password().toStdString(), m_res->getTimeDrift());

    _onvifPtz__GetConfigurations request;
    _onvifPtz__GetConfigurationsResponse response;
    if (ptz.doGetConfigurations(request, response) == SOAP_OK && response.PTZConfiguration.size() > 0)
    {
        m_ptzConfigurationToken = QString::fromStdString(response.PTZConfiguration[0]->token);
        //qCritical() << "reading PTZ configuration success. token=" << m_ptzConfigurationToken;
    }
    else {
        //qCritical() << "reading PTZ configuration failed. " << ptz.getLastError();
        return;
    } 


    _onvifPtz__GetNode nodeRequest;
    _onvifPtz__GetNodeResponse nodeResponse;
    nodeRequest.NodeToken = response.PTZConfiguration[0]->NodeToken;

    if (ptz.doGetNode(nodeRequest, nodeResponse) == SOAP_OK)
    {
        qCritical() << "reading PTZ token success";
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

    //qCritical() << "reading PTZ token finished. minX=" << m_xNativeVelocityCoeff.second;
}

int QnOnvifPtzController::stopMove()
{
    QAuthenticator auth(m_res->getAuth());
    PtzSoapWrapper ptz (m_res->getPtzfUrl().toStdString().c_str(), auth.user().toStdString(), auth.password().toStdString(), m_res->getTimeDrift());
    _onvifPtz__Stop request;
    _onvifPtz__StopResponse response;

    {
        QMutexLocker lock(&m_mutex);
        request.ProfileToken = m_mediaProfile.toStdString();
    }    
    bool StopValue = true;
    request.PanTilt = &StopValue;
    request.Zoom = &StopValue;

    int rez = ptz.doStop(request, response);
    if (rez != SOAP_OK)
    {
        qCritical() << "Error executing PTZ stop command for resource " << m_res->getUniqueId() << ". Error: " << ptz.getLastError();
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

int QnOnvifPtzController::startMove(qreal xVelocity, qreal yVelocity, qreal zoomVelocity)
{
    QAuthenticator auth(m_res->getAuth());
    PtzSoapWrapper ptz (m_res->getPtzfUrl().toStdString().c_str(), auth.user().toStdString(), auth.password().toStdString(), m_res->getTimeDrift());
    _onvifPtz__ContinuousMove request;
    _onvifPtz__ContinuousMoveResponse response;

    {
        QMutexLocker lock(&m_mutex);
        request.ProfileToken = m_mediaProfile.toStdString();
    }
    
    request.Velocity = new onvifXsd__PTZSpeed();
    request.Velocity->PanTilt = new onvifXsd__Vector2D();
    request.Velocity->Zoom = new onvifXsd__Vector1D();

    request.Velocity->PanTilt->x = normalizeSpeed(xVelocity, m_xNativeVelocityCoeff, getXVelocityCoeff());
    request.Velocity->PanTilt->y = normalizeSpeed(yVelocity, m_yNativeVelocityCoeff, getYVelocityCoeff());
    request.Velocity->Zoom->x = normalizeSpeed(zoomVelocity, m_zoomNativeVelocityCoeff, getZoomVelocityCoeff());


    int rez = ptz.doContinuousMove(request, response);
    if (rez != SOAP_OK)
    {
        qCritical() << "Error executing PTZ move command for resource " << m_res->getUniqueId() << ". Error: " << ptz.getLastError();
    }

    delete request.Velocity->Zoom;
    delete request.Velocity->PanTilt;
    delete request.Velocity;

    return rez;
}

QString QnOnvifPtzController::getPtzConfigurationToken()
{
    return m_ptzConfigurationToken;
}

void QnOnvifPtzController::setMediaProfileToken(const QString& value)
{
    m_mediaProfile = value;
}
