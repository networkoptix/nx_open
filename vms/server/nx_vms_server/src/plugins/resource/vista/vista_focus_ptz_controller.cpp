
#ifdef ENABLE_ONVIF

#include "vista_focus_ptz_controller.h"

#include <cassert>

#include <nx/network/deprecated/simple_http_client.h>
#include <utils/common/ini_section.h>
#include <utils/common/warnings.h>

#include <common/common_module.h>
#include <core/resource/resource_data.h>
#include <core/resource_management/resource_data_pool.h>

#include "vista_resource.h"

using namespace nx::core;

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
    m_capabilities(Ptz::NoPtzCapabilities),
    m_isMotor(false)
{
    NX_ASSERT(m_resource);

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
        m_capabilities |= Ptz::AuxiliaryPtzCapability;
        m_traits.push_back(Ptz::ManualAutoFocusPtzTrait);
    }

    if(options.contains(lit("BUILTIN_PTZ"))) {
        m_capabilities |= Ptz::ContinuousFocusCapability;
        m_isMotor = false;
    }

    if(options.contains(lit("PTZ")) && config.value<QString>(lit("device_name")).compare(lit("EncoderVistaPTZ"), Qt::CaseInsensitive) == 0) { // TODO: #Elric the device_name part should go to json settings.
        m_capabilities |= Ptz::ContinuousFocusCapability;
        m_isMotor = false;
    }

    if(options.contains(lit("PTZ"))) {
        QnResourceData data = m_resource->resourceData();
        Ptz::Capabilities extraCaps = Ptz::NoPtzCapabilities;
        data.value(ResourceDataKey::kOperationalPtzCapabilities, &extraCaps);
        if(extraCaps & Ptz::ContinuousFocusCapability) {
            m_capabilities |= Ptz::ContinuousFocusCapability;
            m_isMotor = false;
        }
    }

    if(options.contains(lit("M_PTZ"))) {
        QString ptzOptions = config.value<QString>(lit("m_ptz_option"));
        if(ptzOptions.contains(lit("FOCUS"))) {
            m_capabilities |= Ptz::ContinuousFocusCapability;
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

Ptz::Capabilities QnVistaFocusPtzController::getCapabilities(
    const nx::core::ptz::Options& options) const
{
    if (options.type != ptz::Type::operational)
        return Ptz::NoPtzCapabilities;

    return base_type::getCapabilities(options) | m_capabilities;
}

bool QnVistaFocusPtzController::continuousFocus(
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

    if(!(m_capabilities & Ptz::ContinuousFocusCapability))
        return base_type::continuousFocus(speed, options);

    QnMutexLocker locker( &m_mutex );
    if(m_isMotor) {
        return queryLocked(lit("mptz/control.php?focus=%1").arg(vistaFocusDirection(speed)));
    } else {
        return queryLocked(lit("ptz/control.php?ch=1&focus=%1").arg(vistaFocusDirection(speed)));
    }

}

bool QnVistaFocusPtzController::getAuxiliaryTraits(
    QnPtzAuxiliaryTraitList *auxiliaryTraits,
    const nx::core::ptz::Options& options) const
{
    if(!(m_capabilities & Ptz::AuxiliaryPtzCapability))
        return base_type::getAuxiliaryTraits(auxiliaryTraits, options);

    base_type::getAuxiliaryTraits(auxiliaryTraits, options);
    auxiliaryTraits->append(m_traits);
    return true;
}

bool QnVistaFocusPtzController::runAuxiliaryCommand(
    const QnPtzAuxiliaryTrait &trait,
    const QString &data,
    const nx::core::ptz::Options& options)
{
    if(!(m_capabilities & Ptz::AuxiliaryPtzCapability))
        return base_type::runAuxiliaryCommand(trait, data, options);

    if(trait.standardTrait() == Ptz::ManualAutoFocusPtzTrait) {
        QnMutexLocker locker( &m_mutex );
        return queryLocked(lit("live/live_control.php?smart_focus=1"));
    } else {
        return base_type::runAuxiliaryCommand(trait, data, options);
    }
}

#endif

