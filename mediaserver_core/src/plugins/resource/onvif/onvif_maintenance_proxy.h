#pragma once

#ifdef ENABLE_ONVIF

#include <QtNetwork/QAuthenticator>

#include <utils/camera/camera_diagnostics.h>
#include "soap_wrapper.h"

class DeviceSoapWrapper;

//
// QnOnvifMaintenanceProxy
//
class QnOnvifMaintenanceProxy {
public:
    QnOnvifMaintenanceProxy(
        const SoapTimeouts& timeouts,
        const QString& maintenanceUrl,
        const QAuthenticator& auth,
        const QString& videoSrcToken,
        int timeDrift);
    ~QnOnvifMaintenanceProxy() = default;

    QSet<QString> supportedParameters() const;

    bool callOperation(const QString &id);
private:
    const QString m_maintenanceUrl;
    const QAuthenticator m_auth;
    const QString m_videoSrcToken;
    const int m_timeDrift;
    QSet<QString> m_supportedOperations;
    QScopedPointer<DeviceSoapWrapper> m_deviceSoapWrapper;
};

#endif //ENABLE_ONVIF
