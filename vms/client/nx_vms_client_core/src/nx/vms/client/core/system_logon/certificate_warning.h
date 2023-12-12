// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <QtCore/QObject>

namespace nx::vms::api { struct ModuleInformation; }

namespace nx::network { class SocketAddress; }

namespace nx::vms::client::core {

/** Helper class. Contains text functions and enums. */
class NX_VMS_CLIENT_CORE_API CertificateWarning: public QObject
{
    Q_OBJECT

public:
    enum class Reason
    {
        unknownServer,
        invalidCertificate,
        serverCertificateChanged,
    };

    enum class ClientType
    {
        desktop,
        mobile
    };

    static QString header(
        Reason reason,
        const nx::vms::api::ModuleInformation& info,
        int numberOfServers);

    static QString details(
        Reason reason,
        ClientType clientType = ClientType::desktop);

    static QString advice(
        Reason reason,
        ClientType clientType = ClientType::desktop);

    static QString invalidCertificateError();

private:
    CertificateWarning() = default;
};

} // namespace nx::vms::client::core
