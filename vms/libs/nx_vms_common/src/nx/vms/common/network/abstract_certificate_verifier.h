// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <QtCore/QObject>

#include <nx/network/socket_factory.h>
#include <nx/utils/uuid.h>

namespace nx::vms::common {

/**
 * Generic interface for the certificate verification classes. Currently has two implementations:
 * for the server-server interaction and for the client-server one. Between servers we are using
 * self-signed certificates, stored in the database. Client also supports those as well as
 * user-provided certificates. In suspicious cases it suggests to accept the certificate using a UI
 * dialog.
 */
class NX_VMS_COMMON_API AbstractCertificateVerifier: public QObject
{
public:
    AbstractCertificateVerifier(const std::string& trustedCertificateFilterRegex,
        QObject* parent = nullptr);
    virtual ~AbstractCertificateVerifier();

    virtual nx::network::ssl::AdapterFunc makeAdapterFunc(const QnUuid& serverId) = 0;

protected:
    void loadTrustedCertificate(const QByteArray& data, const QString& name);
};

} // nx::vms::common
