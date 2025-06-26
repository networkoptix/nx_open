// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "abstract_certificate_verifier.h"

#include <QtCore/QDir>
#include <QtCore/QFile>
#include <QtNetwork/QSslConfiguration>

#include <nx/kit/ini_config.h>
#include <nx/network/ssl/certificate.h>

namespace nx::vms::common {

namespace {

struct SecurityIni: nx::kit::IniConfig
{
    SecurityIni(): IniConfig("nx_security.ini") { reload(); }

    NX_INI_STRING("", untrustedCertificatesRegex,
        "ECMAScript regex to ignore built-in TLS certificates by their filenames.");

    NX_INI_FLAG(1, ignoreNonSelfSignedCertificates,
        "Ignore non-self signed (e.g. cross-signed) built-in TLS certificates.");
};

SecurityIni& securityIni()
{
    static SecurityIni ini;
    return ini;
}

} // namespace

static std::once_flag trustedCertificatesLoaded;

AbstractCertificateVerifier::AbstractCertificateVerifier(QObject* parent): QObject(parent)
{
    std::call_once(trustedCertificatesLoaded,
        [this]()
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
            const std::string regexStr(securityIni().untrustedCertificatesRegex);
            const std::regex regex(regexStr);
            const QDir folder(":/trusted_certificates");
            for (const auto& fileInfo: folder.entryInfoList({"*.pem"}, QDir::Files))
            {
                const QString fileName = fileInfo.fileName();
                if (std::regex_match(fileInfo.baseName().toStdString(), regex))
                {
                    NX_INFO(this,
                        "Ignore built-in certificate %1 which matches the filter %2",
                        fileName, regexStr);
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
    const auto chain = nx::network::ssl::Certificate::parse(data.toStdString());
    if (chain.size() != 1)
    {
        NX_WARNING(this,
            "Ignore built-in certificate file %1 with more that one certificate", name);
        return;
    }

    const auto& cert = chain[0];
    if (securityIni().ignoreNonSelfSignedCertificates && cert.issuer() != cert.subject())
    {
        NX_WARNING(this, "Ignore not self-signed built-in certificate %1", name);
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
        NX_WARNING(this, "Ignore expired built-in certificate %1", name);
    }
}

} // nx::vms::common
