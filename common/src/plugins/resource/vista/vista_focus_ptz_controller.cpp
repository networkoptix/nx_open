
#ifdef ENABLE_ONVIF

#include "vista_focus_ptz_controller.h"

#include <cassert>

#include <utils/network/simple_http_client.h>
#include <utils/common/ini_section.h>
#include <utils/common/warnings.h>

#include <common/common_module.h>
#include <core/resource/resource_data.h>
#include <core/resource_management/resource_data_pool.h>

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

} // anonymous namespace


QnVistaFocusPtzController::QnVistaFocusPtzController(const QnPtzControllerPtr &baseController):
    base_type(baseController),
    m_resource(baseController->resource().dynamicCast<QnVistaResource>()),
    m_capabilities(Qn::NoPtzCapabilities),
    m_isMotor(false)
{
    assert(m_resource);

    init();
}

QnVistaFocusPtzController::~QnVistaFocusPtzController() {
    return;
}

void QnVistaFocusPtzController::init() {
    QnIniSection config;

    /* Note that there is no point locking a mutex here as this function is called 
     * only from the constructor. */
    if(!queryLocked(lit("config.txt"), &config)) {
        qnWarning("Could not initialize VISTA PTZ for camera '%1'.", m_resource->getName());        
        return;
    }

    QString options;
    if(!config.value(lit("options"), &options))
        return;

    if(options.contains(lit("SMART_FOCUS"))) {
        m_capabilities |= Qn::AuxilaryPtzCapability;
        m_traits.push_back(Qn::ManualAutoFocusPtzTrait);
    }

    if(options.contains(lit("BUILTIN_PTZ"))) {
        m_capabilities |= Qn::ContinuousFocusCapability;
        m_isMotor = false;
    }

    if(options.contains(lit("PTZ")) && config.value<QString>(lit("device_name")).compare(lit("EncoderVistaPTZ"), Qt::CaseInsensitive) == 0) { // TODO: #Elric the device_name part should go to json settings.
        m_capabilities |= Qn::ContinuousFocusCapability;
        m_isMotor = false;
    }

    if(options.contains(lit("PTZ"))) {
        QnResourceData data = qnCommon->dataPool()->data(m_resource);
        if(data.value<QStringList>(lit("vistaFocusDevices")).contains(config.value<QString>(lit("device_name")), Qt::CaseInsensitive)) {
            m_capabilities |= Qn::ContinuousFocusCapability;
            m_isMotor = false;
        }
    }

    if(options.contains(lit("M_PTZ"))) {
        QString ptzOptions = config.value<QString>(lit("m_ptz_option"));
        if(ptzOptions.contains(lit("FOCUS"))) {
            m_capabilities |= Qn::ContinuousFocusCapability;
            m_isMotor = true;
        }
    }
}

void QnVistaFocusPtzController::ensureClientLocked() {
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

bool QnVistaFocusPtzController::queryLocked(const QString &request, QByteArray *body) {
    ensureClientLocked();

    CLHttpStatus status = m_client->doGET(request.toLatin1());
    if(status == CL_TRANSPORT_ERROR) {
        /* This happens when connection is aborted by the local host. 
         * Http client doesn't handle this case automatically, so we have to work it around. */
        m_client.reset();
        ensureClientLocked();

        status = m_client->doGET(request.toLatin1());
    }
    if(status != CL_HTTP_SUCCESS)
        return false;
    
    if(body)
        m_client->readAll(*body);
    return true;
}

bool QnVistaFocusPtzController::queryLocked(const QString &request, QnIniSection *section, QByteArray *body) {
    QByteArray localBody;
    if(!queryLocked(request, &localBody))
        return false;

    if(section)
        *section = QnIniSection::fromIni(localBody);

    if(body)
        *body = localBody;

    return true;
}

Qn::PtzCapabilities QnVistaFocusPtzController::getCapabilities() {
    return base_type::getCapabilities() | m_capabilities;
}

bool QnVistaFocusPtzController::continuousFocus(qreal speed) {
    if(!(m_capabilities & Qn::ContinuousFocusCapability))
        return base_type::continuousFocus(speed);

    SCOPED_MUTEX_LOCK( locker, &m_mutex);
    if(m_isMotor) {
        return queryLocked(lit("mptz/control.php?focus=%1").arg(vistaFocusDirection(speed)));
    } else {
        return queryLocked(lit("ptz/control.php?ch=1&focus=%1").arg(vistaFocusDirection(speed)));
    }

}

bool QnVistaFocusPtzController::getAuxilaryTraits(QnPtzAuxilaryTraitList *auxilaryTraits) {
    if(!(m_capabilities & Qn::AuxilaryPtzCapability))
        return base_type::getAuxilaryTraits(auxilaryTraits);

    base_type::getAuxilaryTraits(auxilaryTraits);
    auxilaryTraits->append(m_traits);
    return true;
}

bool QnVistaFocusPtzController::runAuxilaryCommand(const QnPtzAuxilaryTrait &trait, const QString &data) {
    if(!(m_capabilities & Qn::AuxilaryPtzCapability))
        return base_type::runAuxilaryCommand(trait, data);

    if(trait.standardTrait() == Qn::ManualAutoFocusPtzTrait) {
        SCOPED_MUTEX_LOCK( locker, &m_mutex);
        return queryLocked(lit("live/live_control.php?smart_focus=1"));
    } else {
        return base_type::runAuxilaryCommand(trait, data);
    }
}

#endif

