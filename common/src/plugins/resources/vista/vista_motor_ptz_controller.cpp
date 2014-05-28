#include "vista_motor_ptz_controller.h"

#include <cassert>

#include <utils/network/simple_http_client.h>
#include <utils/common/ini_section.h>

#include "vista_resource.h"

namespace {

    QString vistaFocusDirection(qreal speed) {
        if(qFuzzyIsNull(speed)) {
            return lit("stop");
        } else if(speed < 0) {
            return lit("near");
        } else {
            return lit("far");
        }
    }

    QString vistaZoomDirection(qreal speed) {
        if(qFuzzyIsNull(speed)) {
            return lit("stop");
        } else if(speed < 0) {
            return lit("wide");
        } else {
            return lit("tele");
        }
    }

} // anonymous namespace


QnVistaMotorPtzController::QnVistaMotorPtzController(const QnVistaResourcePtr &resource):
    base_type(resource),
    m_capabilities(Qn::NoPtzCapabilities),
    m_resource(resource)
{
    assert(m_resource);

    init();
}

QnVistaMotorPtzController::~QnVistaMotorPtzController() {
    return;
}

void QnVistaMotorPtzController::init() {
    return;

    QnIniSection config;

    /* Note that there is no point locking a mutex here as this function is called 
     * only from the constructor. */
    if(!queryLocked(lit("config.txt"), &config)) {
        qnWarning("Could not initialize VISTA MOTOR PTZ for camera '%1'.", m_resource->getName());        
        return;
    }

    QString options;
    if(!config.value(lit("options"), &options))
        return;
    if(!options.contains(lit("M_PTZ")))
        return;

    QString ptzOptions;
    if(!config.value(lit("m_ptz_option"), &ptzOptions))
        return;

    if(ptzOptions.contains(lit("ZOOM")))
        m_capabilities |= Qn::ContinuousZoomCapability;
    if(ptzOptions.contains(lit("FOCUS")))
        m_capabilities |= Qn::ContinuousFocusCapability;
}

void QnVistaMotorPtzController::ensureClientLocked() {
    /* Vista cameras treat closed channel as a stop command. This is why
     * we have to keep the http client instance alive between requests. */

    QString host = m_resource->getHostAddress();
    int timeout = m_resource->getNetworkTimeout();
    QAuthenticator auth = m_resource->getAuth();

    if(m_client && m_lastHostAddress == host && m_client->timeout() == timeout && m_client->auth() == auth)
        return;

    m_lastHostAddress = host;
    m_client.reset(new CLSimpleHTTPClient(host, 80, timeout, auth));
}

bool QnVistaMotorPtzController::queryLocked(const QString &request, QByteArray *body) {
    ensureClientLocked();

    CLHttpStatus status = m_client->doGET(request.toLatin1());
    if(status != CL_HTTP_SUCCESS)
        return false;
    
    if(body)
        m_client->readAll(*body);
    return true;
}

bool QnVistaMotorPtzController::queryLocked(const QString &request, QnIniSection *section, QByteArray *body) {
    QByteArray localBody;
    if(!queryLocked(request, &localBody))
        return false;

    if(section)
        *section = QnIniSection::fromIni(localBody);

    if(body)
        *body = localBody;

    return true;
}

Qn::PtzCapabilities QnVistaMotorPtzController::getCapabilities() {
    return m_capabilities;
}

bool QnVistaMotorPtzController::continuousMove(const QVector3D &speed) {
    if(!(m_capabilities & Qn::ContinuousZoomCapability))
        return false;

    QMutexLocker locker(&m_mutex);
    return queryLocked(lit("mptz/control.php?zoom=%1").arg(vistaZoomDirection(speed.z())));
}

bool QnVistaMotorPtzController::continuousFocus(qreal speed) {
    if(!(m_capabilities & Qn::ContinuousFocusCapability))
        return false;

    QMutexLocker locker(&m_mutex);
    return queryLocked(lit("mptz/control.php?focus=%1").arg(vistaFocusDirection(speed)));
}
