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
        } else if(speed > 0) {
            return lit("far");
        }
    }

    QString vistaZoomDirection(qreal speed) {
        if(qFuzzyIsNull(speed)) {
            return lit("stop");
        } else if(speed < 0) {
            return lit("wide");
        } else if(speed > 0) {
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
    QnIniSection config;
    if(!query(lit("config.txt"), &config)) {
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

CLSimpleHTTPClient *QnVistaMotorPtzController::newHttpClient() const {
    return new CLSimpleHTTPClient(
        m_resource->getHostAddress(), 
        QUrl(m_resource->getUrl()).port(80), 
        m_resource->getNetworkTimeout(),
        m_resource->getAuth()
    );
}

bool QnVistaMotorPtzController::query(const QString &request, QByteArray *body = NULL) const {
    QScopedPointer<CLSimpleHTTPClient> http(newHttpClient());

    CLHttpStatus status = http->doGET(request.toLatin1());
    if(status != CL_HTTP_SUCCESS)
        return false;
    
    if(body)
        http->readAll(*body);
    return true;
}

bool QnVistaMotorPtzController::query(const QString &request, QnIniSection *section, QByteArray *body = NULL) {
    QByteArray localBody;
    if(!query(request, &localBody))
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

    return query(lit("mptz/control.php?zoom=%1").arg(vistaZoomDirection(speed.z())));
}

bool QnVistaMotorPtzController::continuousFocus(qreal speed) {
    if(!(m_capabilities & Qn::ContinuousFocusCapability))
        return false;

    return query(lit("mptz/control.php?focus=%1").arg(vistaFocusDirection(speed)));
}
