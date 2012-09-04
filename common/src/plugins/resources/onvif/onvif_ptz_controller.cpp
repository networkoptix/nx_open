#include "onvif_ptz_controller.h"
#include "plugins/resources/onvif/onvif_resource.h"
#include "soap_wrapper.h"
#include "onvif/soapDeviceBindingProxy.h"

QnOnvifPtzController::QnOnvifPtzController(QnResourcePtr res, const QString& mediaProfile): QnAbstractPtzController(res)
{
    m_res = qSharedPointerDynamicCast<QnPlOnvifResource> (res);
    m_xNativeVelocityCoeff.first = m_xNativeVelocityCoeff.second = 1.0;
    m_yNativeVelocityCoeff.first = m_yNativeVelocityCoeff.second = 1.0;
    m_zoomNativeVelocityCoeff.first = m_zoomNativeVelocityCoeff.second = 1.0;

    // read PTZ params

    _onvifPtz__GetNodes request;
    _onvifPtz__GetNodesResponse response;
    QAuthenticator auth(m_res->getAuth());
    PtzSoapWrapper ptz (m_res->getMediaUrl().toStdString().c_str(), auth.user().toStdString(), auth.password().toStdString(), m_res->getTimeDrift());
    if (ptz.doGetNodes(request, response) == SOAP_OK)
    {
        if (response.PTZNode.size() > 0) 
        {
            onvifXsd__PTZNode* ptzNode = response.PTZNode[0];
            m_ptzToken = QString::fromStdString(ptzNode[0].token);
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
    m_mediaProfile = mediaProfile;
}

int QnOnvifPtzController::stopMove()
{
    QAuthenticator auth(m_res->getAuth());
    PtzSoapWrapper ptz (m_res->getMediaUrl().toStdString().c_str(), auth.user().toStdString(), auth.password().toStdString(), m_res->getTimeDrift());
    _onvifPtz__Stop request;
    _onvifPtz__StopResponse response;

    {
        QMutexLocker lock(&m_mutex);
        request.ProfileToken = m_mediaProfile.toStdString();
    }    

    int rez = ptz.doStop(request, response);
    if (rez != SOAP_OK)
    {
        qCritical() << "Error executing PTZ stop command for resource " << m_res->getUniqueId() << ". Error: " << ptz.getLastError();
    }
    return rez;
}

float QnOnvifPtzController::normalizeSpeed(qreal inputVelocity, const QPair<qreal, qreal>& nativeCoeff, qreal userCoeff)
{
    inputVelocity *= inputVelocity >= 0 ? nativeCoeff.first : -nativeCoeff.second;
    inputVelocity *= userCoeff;
    return qBound(nativeCoeff.second, inputVelocity, nativeCoeff.first);
}

int QnOnvifPtzController::startMove(qreal xVelocity, qreal yVelocity, qreal zoomVelocity)
{
    QAuthenticator auth(m_res->getAuth());
    PtzSoapWrapper ptz (m_res->getMediaUrl().toStdString().c_str(), auth.user().toStdString(), auth.password().toStdString(), m_res->getTimeDrift());
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

QString QnOnvifPtzController::getPtzNodeToken() const
{
    return m_ptzToken;
}

QString QnOnvifPtzController::getPtzConfigurationToken()
{
    if (!m_ptzConfigurationToken.isEmpty())
        return m_ptzConfigurationToken;

    QAuthenticator auth(m_res->getAuth());
    MediaSoapWrapper media (m_res->getMediaUrl().toStdString().c_str(), auth.user().toStdString(), auth.password().toStdString(), m_res->getTimeDrift());
    CompatibleMetadataConfiguration request;
    CompatibleMetadataConfigurationResp response;
    {
        QMutexLocker lock(&m_mutex);
        request.ProfileToken = m_mediaProfile.toStdString();
    }
    if (media.getCompatibleMetadataConfigurations(request, response) == SOAP_OK)
    {
        if (response.Configurations.size() > 0)
            m_ptzConfigurationToken = QString::fromStdString(response.Configurations[0]->token);
        //m_ptzConfigurationToken = response.
    }
    return m_ptzConfigurationToken;
}

void QnOnvifPtzController::setMediaProfileToken(const QString& value)
{
    QMutexLocker lock(&m_mutex);
    m_mediaProfile = value;
}
