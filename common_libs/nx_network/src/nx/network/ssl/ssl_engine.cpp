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
        NX_LOG("SSL: Unable to generate big number", cl_logWARNING);
        return String();
    }

    auto rsa = utils::wrapUnique(RSA_new(), &RSA_free);
    if (!rsa || !RSA_generate_key_ex(rsa.get(), kRsaLength, number.get(), NULL))
    {
        NX_LOG("SSL: Unable to generate RSA", cl_logWARNING);
        return String();
    }

    auto pkey = utils::wrapUnique(EVP_PKEY_new(), &EVP_PKEY_free);
    if (!pkey || !EVP_PKEY_assign_RSA(pkey.get(), rsa.release()))
    {
        NX_LOG("SSL: Unable to generate PKEY", cl_logWARNING);
        return String();
    }

    auto x509 = utils::wrapUnique(X509_new(), &X509_free);
    if (!x509
        || !ASN1_INTEGER_set(X509_get_serialNumber(x509.get()), serialNumber)
        || !X509_gmtime_adj(X509_get_notBefore(x509.get()), 0)
        || !X509_gmtime_adj(X509_get_notAfter(x509.get()), kCertExpiration.count())
        || !X509_set_pubkey(x509.get(), pkey.get()))
    {
        NX_LOG("SSL: Unable to generate X509 cert", cl_logWARNING);
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
        NX_LOG("SSL: Unable to sign X509 cert", cl_logWARNING);
        return String();
    }

    char writeBuffer[kBufferSize] = { 0 };
    const auto bio = utils::wrapUnique(BIO_new(BIO_s_mem()), BIO_free);
    if (!bio
        || !PEM_write_bio_PrivateKey(bio.get(), pkey.get(), 0, 0, 0, 0, 0)
        || !PEM_write_bio_X509(bio.get(), x509.get())
        || !BIO_read(bio.get(), writeBuffer, kBufferSize))
    {
        NX_LOG("SSL: Unable to generate cert string", cl_logWARNING);
        return String();
    }

    return String(writeBuffer);
}

static String x509info(X509& x509)
{
    auto issuer = utils::wrapUnique(X509_NAME_oneline(
        X509_get_issuer_name(&x509), NULL, 0), &CRYPTO_free);

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
        NX_LOG("SSL: Unable to read primary X509", cl_logDEBUG1);
        return false;
    }

    size_t chainSize = (size_t)certSize;
    if (!SSL_CTX_use_certificate(sslData->serverContext(), x509.get()))
    {
        NX_LOG(lm("SSL: Unable to use primary X509: %1").arg(x509info(*x509)), cl_logWARNING);
        return false;
    }

    NX_LOG(lm("SSL: Primary X509 is loaded: %1").arg(x509info(*x509.get())), cl_logINFO);
    while (true)
    {
        x509 = utils::wrapUnique(PEM_read_bio_X509_AUX(bio.get(), 0, 0, 0), &X509_free);
        certSize = i2d_X509(x509.get(), 0);
        if (!x509 || certSize <= 0)
            return true;

        chainSize += (size_t)certSize;
        if (chainSize > maxChainSize)
        {
            NX_ASSERT(false, "SSL: Certificate chain leght is too long");
            return true;
        }

        if (SSL_CTX_add_extra_chain_cert(sslData->serverContext(), x509.get()))
        {
            NX_LOG(lm("SSL: Chained X509 is loaded: %1").arg(x509info(*x509)), cl_logINFO);
            x509.release();
        }
        else
        {
            NX_LOG(lm("SSL: Unable to load chained X509: %1").arg(x509info(*x509)), cl_logWARNING);
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
        NX_LOG("SSL: Unable to read PKEY", cl_logDEBUG1);
        return false;
    }

    if (!SSL_CTX_use_PrivateKey(sslData->serverContext(), sslData->pkey.get()))
    {
        NX_LOG("SSL: Unable to use PKEY", cl_logWARNING);
        return false;
    }

    NX_LOG("SSL: PKEY is loaded (SSL init is complete)", cl_logINFO);
    return true;
}

bool Engine::useCertificateAndPkey(const String& certData)
{
    const auto sslData = ssl::SslStaticData::instance();
    return x509load(sslData, certData) && pKeyLoad(sslData, certData);
}

void Engine::useOrCreateCertificate(
    const QString& filePath,
    const String& name, const String& country, const String& company)
{
    String certData;
    QFile file(filePath);
    if (filePath.isEmpty()
        || !file.open(QIODevice::ReadOnly)
        || (certData = file.readAll()).isEmpty())
    {
        file.close();
        NX_LOG(lm("SSL: Unable to find valid SSL certificate '%1', generate new one")
            .arg(filePath), cl_logALWAYS);

        certData = makeCertificateAndKey(name, country, company);

        NX_ASSERT(!certData.isEmpty());
        if (!filePath.isEmpty())
        {
            QDir(filePath).mkpath(lit(".."));
            if (!file.open(QIODevice::WriteOnly) ||
                file.write(certData) != certData.size())
            {
                NX_LOG("SSL: Unable to write SSL certificate to file", cl_logERROR);
            }

            file.close();
        }
    }

    NX_LOG(lm("SSL: Load certificate from '%1'").arg(filePath), cl_logINFO);
    useCertificateAndPkey(certData);
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
