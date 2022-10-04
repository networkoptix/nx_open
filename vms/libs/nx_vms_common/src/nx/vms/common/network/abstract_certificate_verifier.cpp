// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "abstract_certificate_verifier.h"

#include <QtCore/QDir>
#include <QtCore/QFile>
#include <QtNetwork/QSslConfiguration>

#include <nx/network/ssl/certificate.h>

namespace nx::vms::common {

static std::once_flag trustedCertificatesLoaded;

AbstractCertificateVerifier::AbstractCertificateVerifier(QObject* parent): QObject(parent)
{
    std::call_once(trustedCertificatesLoaded,
        [this]()
        {
            constexpr auto kCaLogLevel = nx::utils::log::Level::verbose;
            if (nx::utils::log::isToBeLogged(kCaLogLevel))
            {
                NX_UTILS_LOG(kCaLogLevel, this, "OS CA for now %1", nx::utils::utcTime());
                for (const auto& c: QSslConfiguration::defaultConfiguration().caCertificates())
                    NX_UTILS_LOG(kCaLogLevel, this, c);
            }
            const QDir folder(":/trusted_certificates");
            for (const auto& fileName: folder.entryList({"*.pem"}, QDir::Files))
            {
                QFile f(folder.absoluteFilePath(fileName));
                if (!f.open(QIODevice::ReadOnly | QIODevice::Text))
                {
                    NX_WARNING(this, "Cannot open file %1", folder.absoluteFilePath(fileName));
                    continue;
                }
                loadTrustedCertificate(f.readAll(), fileName);
            }
        });
}

AbstractCertificateVerifier::~AbstractCertificateVerifier() {}

void AbstractCertificateVerifier::loadTrustedCertificate(
    const QByteArray& data, const QString& name)
{
    const auto chain = nx::network::ssl::Certificate::parse(data.toStdString(),
        /*assertOnFail*/ false);
    if (chain.size() != 1)
    {
        NX_VERBOSE(this, "The file does not contain a single certificate: %1", name);
        return;
    }

    const auto& cert = chain[0];
    if (cert.issuer() != cert.subject())
    {
        NX_VERBOSE(this, "Certificate is not self-signed: %1", name);
        return;
    }

    const auto now = nx::utils::utcTime();
    if (now >= cert.notBefore() && now <= cert.notAfter())
    {
        NX_VERBOSE(this, "Adding certificate into the trusted CA list: %1", name);
        nx::network::ssl::addTrustedRootCertificate(cert);
    }
    else
    {
        NX_VERBOSE(this, "Ignore expired certificate: %1", name);
    }
}

} // nx::vms::common
