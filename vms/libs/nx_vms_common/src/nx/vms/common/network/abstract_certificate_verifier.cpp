// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "abstract_certificate_verifier.h"

#include <QtCore/QDir>
#include <QtCore/QFile>
#include <QtNetwork/QSslConfiguration>

#include <nx/network/ssl/certificate.h>

namespace nx::vms::common {

static std::once_flag trustedCertificatesLoaded;

AbstractCertificateVerifier::AbstractCertificateVerifier(
    const std::string& trustedCertificateFilterRegex,
    QObject* parent)
    :
    QObject(parent)
{
    std::call_once(trustedCertificatesLoaded,
        [this, trustedCertificateFilterRegex]()
        {
            constexpr auto kCaLogLevel = nx::log::Level::verbose;
            if (nx::log::isToBeLogged(kCaLogLevel))
            {
                NX_UTILS_LOG(kCaLogLevel, this, "OS CA for now %1", nx::utils::utcTime());
                NX_UTILS_LOG(kCaLogLevel, this, "Certificates:");
                for (const auto& c: QSslConfiguration::defaultConfiguration().caCertificates())
                    NX_UTILS_LOG(kCaLogLevel, this, c);
                NX_UTILS_LOG(kCaLogLevel, this, "System certificates:");
                for (const auto& c: QSslConfiguration::defaultConfiguration().systemCaCertificates())
                    NX_UTILS_LOG(kCaLogLevel, this, c);
            }
            const std::regex regex(trustedCertificateFilterRegex);
            const QDir folder(":/trusted_certificates");
            for (const auto& fileInfo: folder.entryInfoList({"*.pem"}, QDir::Files))
            {
                const QString fileName = fileInfo.fileName();
                if (!std::regex_match(fileInfo.baseName().toStdString(), regex))
                {
                    NX_INFO(this,
                        "Ignore trusted certificate %1 which does not match the filter %2",
                        fileName, trustedCertificateFilterRegex);
                    continue;
                }

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
        NX_WARNING(this,
            "Ignore trusted certificate %1 with more that one certificate", name);
        return;
    }

    const auto& cert = chain[0];
    if (cert.issuer() != cert.subject())
    {
        NX_WARNING(this, "Ignore not self-signed trusted certificate %1", name);
        return;
    }

    const auto now = nx::utils::utcTime();
    if (now >= cert.notBefore() && now <= cert.notAfter())
    {
        NX_INFO(this, "Adding trusted certificate %1 into the CA list", name);
        nx::network::ssl::addTrustedRootCertificate(cert);
    }
    else
    {
        NX_WARNING(this, "Ignore expired trusted certificate %1", name);
    }
}

} // nx::vms::common
