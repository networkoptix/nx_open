#include "ssl_engine.h"

#ifdef ENABLE_SSL

#include <QtCore/QDir>

#include <nx/utils/log/log.h>
#include <nx/utils/random.h>
#include <nx/utils/type_utils.h>

#include "ssl_static_data.h"

namespace nx {
namespace network {
namespace ssl {

const size_t Engine::kBufferSize = 1024 * 10;
const int Engine::kRsaLength = 2048;
const std::chrono::seconds Engine::kCertExpiration =
    std::chrono::hours(5 * 365 * 24); // 5 years

String Engine::makeCertificateAndKey(
    const String& common, const String& country, const String& company)
{
    ssl::SslStaticData::instance();
    const int serialNumber = nx::utils::random::number();

    auto number = utils::wrapUnique(BN_new(), &BN_free);
    if (!number || !BN_set_word(number.get(), RSA_F4))
    {
        NX_WARNING(typeid(Engine), "Unable to generate big number");
        return String();
    }

    auto rsa = utils::wrapUnique(RSA_new(), &RSA_free);
    if (!rsa || !RSA_generate_key_ex(rsa.get(), kRsaLength, number.get(), NULL))
    {
        NX_WARNING(typeid(Engine), "Unable to generate RSA");
        return String();
    }

    auto pkey = utils::wrapUnique(EVP_PKEY_new(), &EVP_PKEY_free);
    if (!pkey || !EVP_PKEY_assign_RSA(pkey.get(), rsa.release()))
    {
        NX_WARNING(typeid(Engine), "Unable to generate PKEY");
        return String();
    }

    auto x509 = utils::wrapUnique(X509_new(), &X509_free);
    if (!x509
        || !ASN1_INTEGER_set(X509_get_serialNumber(x509.get()), serialNumber)
        || !X509_gmtime_adj(X509_get_notBefore(x509.get()), 0)
        || !X509_gmtime_adj(X509_get_notAfter(x509.get()), kCertExpiration.count())
        || !X509_set_pubkey(x509.get(), pkey.get()))
    {
        NX_WARNING(typeid(Engine), "Unable to generate X509 cert");
        return String();
    }

    const auto name = X509_get_subject_name(x509.get());
    const auto nameSet = [&name](const char* field, const String& value)
    {
        auto vptr = (unsigned char *)value.data();
        return X509_NAME_add_entry_by_txt(
            name, field, MBSTRING_UTF8, vptr, -1, -1, 0);
    };

    if (!name
        || !nameSet("C", country) || !nameSet("O", company) || !nameSet("CN", common)
        || !X509_set_issuer_name(x509.get(), name)
        || !X509_sign(x509.get(), pkey.get(), EVP_sha256()))
    {
        NX_WARNING(typeid(Engine), "Unable to sign X509 cert");
        return String();
    }

    char writeBuffer[kBufferSize] = { 0 };
    const auto bio = utils::wrapUnique(BIO_new(BIO_s_mem()), BIO_free);
    if (!bio
        || !PEM_write_bio_PrivateKey(bio.get(), pkey.get(), 0, 0, 0, 0, 0)
        || !PEM_write_bio_X509(bio.get(), x509.get())
        || !BIO_read(bio.get(), writeBuffer, kBufferSize))
    {
        NX_WARNING(typeid(Engine), "Unable to generate cert string");
        return String();
    }

    return String(writeBuffer);
}

static String x509info(X509& x509)
{
    auto issuer = utils::wrapUnique(X509_NAME_oneline(
        X509_get_issuer_name(&x509), NULL, 0), [](char* ptr) { free(ptr); });

    if (!issuer || !issuer.get())
        return String("unavaliable");

    String info(issuer.get() + 1);
    return info.replace("/", ", ");
}

static bool x509load(ssl::SslStaticData* sslData, const Buffer& certBytes)
{
    const size_t maxChainSize = (size_t)SSL_CTX_get_max_cert_list(sslData->serverContext());
    auto bio = utils::wrapUnique(
        BIO_new_mem_buf(const_cast<void*>((const void*)certBytes.data()), certBytes.size()),
        &BIO_free);

    auto x509 = utils::wrapUnique(PEM_read_bio_X509_AUX(bio.get(), 0, 0, 0), &X509_free);
    auto certSize = i2d_X509(x509.get(), 0);
    if (!x509 || certSize <= 0)
    {
        NX_DEBUG(typeid(Engine), "Unable to read primary X509");
        return false;
    }

    size_t chainSize = (size_t)certSize;
    if (!SSL_CTX_use_certificate(sslData->serverContext(), x509.get()))
    {
        NX_WARNING(typeid(Engine), lm("Unable to use primary X509: %1").arg(x509info(*x509)));
        return false;
    }

    NX_INFO(typeid(Engine), lm("Primary X509 is loaded: %1").arg(x509info(*x509.get())));
    while (true)
    {
        x509 = utils::wrapUnique(PEM_read_bio_X509_AUX(bio.get(), 0, 0, 0), &X509_free);
        certSize = i2d_X509(x509.get(), 0);
        if (!x509 || certSize <= 0)
            return true;

        chainSize += (size_t)certSize;
        if (chainSize > maxChainSize)
        {
            NX_ASSERT(false, "Certificate chain is too long");
            return true;
        }

        if (SSL_CTX_add_extra_chain_cert(sslData->serverContext(), x509.get()))
        {
            NX_INFO(typeid(Engine), lm("Chained X509 is loaded: %1").arg(x509info(*x509)));
            x509.release();
        }
        else
        {
            NX_WARNING(typeid(Engine), lm("Unable to load chained X509: %1").arg(x509info(*x509)));
            return false;
        }
    }
}

static bool pKeyLoad(ssl::SslStaticData* sslData, const Buffer& certBytes)
{
    auto bio = utils::wrapUnique(
        BIO_new_mem_buf(const_cast<void*>((const void*)certBytes.data()), certBytes.size()),
        &BIO_free);

    sslData->pkey.reset(PEM_read_bio_PrivateKey(bio.get(), 0, 0, 0));
    if (!sslData->pkey)
    {
        NX_DEBUG(typeid(Engine), "Unable to read PKEY");
        return false;
    }

    if (!SSL_CTX_use_PrivateKey(sslData->serverContext(), sslData->pkey.get()))
    {
        NX_WARNING(typeid(Engine), "Unable to use PKEY");
        return false;
    }

    NX_INFO(typeid(Engine), "PKEY is loaded (SSL init is complete)");
    return true;
}

bool Engine::useCertificateAndPkey(const String& certData)
{
    const auto sslData = ssl::SslStaticData::instance();
    return x509load(sslData, certData) && pKeyLoad(sslData, certData);
}

bool Engine::useOrCreateCertificate(
    const QString& filePath,
    const String& name, const String& country, const String& company)
{
    if (loadCertificateFromFile(filePath))
    {
        NX_INFO(typeid(Engine), "Loaded certificate from '%1'", filePath);
        return true;
    }

    NX_ALWAYS(typeid(Engine), "Unable to find valid SSL certificate '%1', generate new one",
        filePath);

    const auto certData = makeCertificateAndKey(name, country, company);

    NX_ASSERT(!certData.isEmpty());
    if (!filePath.isEmpty())
    {
        QFileInfo(filePath).absoluteDir().mkpath(".");
        QFile file(filePath);
        if (!file.open(QIODevice::WriteOnly) || file.write(certData) != certData.size())
        {
            NX_ERROR(typeid(Engine), "Unable to write SSL certificate to file %1: %2",
                filePath, file.errorString());
        }
    }

    NX_INFO(typeid(Engine), "Using auto-generated certificate");
    return useCertificateAndPkey(certData);
}

bool Engine::loadCertificateFromFile(const QString& filePath)
{
    if (filePath.isEmpty())
    {
        NX_INFO(typeid(Engine), "Certificate path is empty");
        return false;
    }

    String certData;
    QFile file(filePath);
    if (!file.open(QIODevice::ReadOnly))
    {
        NX_INFO(typeid(Engine), "Failed to open certificate file '%1': %2",
            filePath, file.errorString());
        return false;
    }
    certData = file.readAll();

    NX_INFO(typeid(Engine), "Loaded certificate from '%1'", filePath);
    if (!useCertificateAndPkey(certData))
    {
        NX_INFO(typeid(Engine), "Failed to load certificate from '%1'", filePath);
        return false;
    }

    NX_INFO(typeid(Engine), "Used certificate from '%1'", filePath);
    return true;
}

void Engine::useRandomCertificate(const String& module)
{
    const auto sslCert = makeCertificateAndKey(
        module, "US", "Network Optix");

    NX_CRITICAL(!sslCert.isEmpty());
    NX_CRITICAL(useCertificateAndPkey(sslCert));
}

void Engine::setAllowedServerVersions(const String& versions)
{
    ssl::SslStaticData::setAllowedServerVersions(versions);
}

void Engine::setAllowedServerCiphers(const String& ciphers)
{
    ssl::SslStaticData::setAllowedServerCiphers(ciphers);
}

} // namespace ssl
} // namespace network
} // namespace nx

#endif // ENABLE_SSL
