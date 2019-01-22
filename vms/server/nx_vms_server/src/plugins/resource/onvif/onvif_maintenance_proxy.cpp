#include "onvif_maintenance_proxy.h"

#ifdef ENABLE_ONVIF

#include <plugins/resource/onvif/soap_wrapper.h>

#include "onvif/soapDeviceBindingProxy.h"
#include "onvif/soapStub.h"

namespace {

    const QString rebootId = lit("mReboot");
    const QString softResetId = lit("mSoftReset");
    const QString hardResetId = lit("mHardReset");

}

QnOnvifMaintenanceProxy::QnOnvifMaintenanceProxy(
    const SoapTimeouts& timeouts,
    const QString& maintenanceUrl,
    const QAuthenticator& auth,
    int timeDrift)
    :
    m_maintenanceUrl(maintenanceUrl),
    m_auth(auth),
    m_timeDrift(timeDrift),
    m_deviceSoapWrapper(new DeviceSoapWrapper(
        timeouts, maintenanceUrl.toLatin1().data(), auth.user(), auth.password(), timeDrift))
{
    m_supportedOperations << rebootId << softResetId << hardResetId;
}

QSet<QString> QnOnvifMaintenanceProxy::supportedParameters() const
{
    return m_supportedOperations;
}

bool QnOnvifMaintenanceProxy::callOperation(const QString &id) {

    if (!m_supportedOperations.contains(id))
        return false;

    if (id == rebootId) {
        RebootReq request;
        RebootResp response;

        int soapRes = m_deviceSoapWrapper->systemReboot(request, response);
        if (soapRes != SOAP_OK) {
            qWarning() << "MaintenanceSystemRebootOperation::set: can't perform reboot on camera."
                << ". Reason: SOAP to endpoint " << m_deviceSoapWrapper->endpoint() << " failed. GSoap error code: "
                << soapRes << ". " << m_deviceSoapWrapper->getLastErrorDescription();
            return false;
        }

        return true;
    } else if (id == softResetId) {
        FactoryDefaultReq request;
        FactoryDefaultResp response;

        int soapRes = m_deviceSoapWrapper->setSystemFactoryDefaultSoft(request, response);
        if (soapRes != SOAP_OK) {
            qWarning() << "MaintenanceSoftSystemFactoryDefaultOperation::set: can't perform soft factory default on camera. UniqId: "
                << ". Reason: SOAP to endpoint " << m_deviceSoapWrapper->endpoint() << " failed. GSoap error code: "
                << soapRes << ". " << m_deviceSoapWrapper->getLastErrorDescription();
            return false;
        }

        return true;
    } else if (id == hardResetId) {
        FactoryDefaultReq request;
        FactoryDefaultResp response;

        int soapRes = m_deviceSoapWrapper->setSystemFactoryDefaultHard(request, response);
        if (soapRes != SOAP_OK) {
            qWarning() << "MaintenanceSoftSystemFactoryDefaultOperation::set: can't perform hard factory default on camera. UniqId: "
                << ". Reason: SOAP to endpoint " << m_deviceSoapWrapper->endpoint() << " failed. GSoap error code: "
                << soapRes << ". " << m_deviceSoapWrapper->getLastErrorDescription();
            return false;
        }

        return true;
    }
    return false;

}

#endif
