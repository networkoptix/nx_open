#include "ssl_engine.h"

#ifdef ENABLE_SSL

#include <openssl/x509v3.h>

#include <QtCore/QDir>

#include <nx/utils/log/log.h>
#include <nx/utils/random.h>
#include <nx/utils/type_utils.h>

#include "ssl_static_data.h"

namespace nx {
namespace network {
namespace ssl {

static const size_t kBufferSize = 1024 * 10;
static const int kRsaLength = 2048;

// https://cabforum.org/2017/03/17/ballot-193-825-day-certificate-lifetimes/
// https://support.apple.com/en-us/HT211025
static const long kCertMaxDurationS = (long) std::chrono::seconds(std::chrono::hours(24) * 297).count();

static bool setDetails(X509* x509, const Engine::CertDetails& details)
{
    const auto name = X509_get_subject_name(x509);
    const auto setField = [&name](const char* field, const QString& value)
    {
        const String utf8value = value.toUtf8();
        return X509_NAME_add_entry_by_txt(
            name, field, MBSTRING_UTF8, (unsigned char *) utf8value.data(), -1, -1, 0);
    };

    return name
        && setField("C", details.country)
        && setField("O", details.organization)
        && setField("CN", details.commonName)
        && X509_set_issuer_name(x509, name);
}

static QString rawDetails(X509* x509)
{
    auto issuer = utils::wrapUnique(X509_NAME_oneline(
        X509_get_issuer_name(x509), NULL, 0), [](char* ptr) { free(ptr); });

    if (!issuer || !issuer.get())
        return String("unable to read details");

    String info(issuer.get() + 1);
    return info.replace("/", ", ");
}

static bool isSelfSigned(X509* x509, const Engine::CertDetails& ownDetails)
{
    const auto details = rawDetails(x509);
    return !ownDetails.country.isEmpty() && details.contains("C=" + ownDetails.country)
        && !ownDetails.organization.isEmpty() && details.contains("O=" + ownDetails.organization)
        && !ownDetails.commonName.isEmpty() && details.contains("CN=" + ownDetails.commonName);
}

static QString toString(ASN1_TIME* time)
{
    const auto bio = nx::utils::wrapUnique(BIO_new(BIO_s_mem()), &BIO_free);
    ASN1_TIME_print(bio.get(), time);

    char buffer[255];
    buffer[BIO_read(bio.get(), buffer, sizeof(buffer) - 1)] = 0;
    return buffer;
}

static bool isExpired(X509* x509)
{
    const auto notBefore = X509_get_notBefore(x509);
    if (notBefore && X509_cmp_time(notBefore, NULL) > 0)
    {
        NX_DEBUG(typeid(Engine), "Certifificate is from the future %1: %2",
            toString(notBefore), rawDetails(x509));
        return true;
    }

    const auto notAfter = X509_get_notAfter(x509);
    if (notAfter && X509_cmp_time(notAfter, NULL) < 0)
    {
        NX_DEBUG(typeid(Engine), "Certifificate is expired %1: %2",
            toString(notAfter), rawDetails(x509));
        return true;
    }

    int durationD = 0, durationS = 0;
    if (!ASN1_TIME_diff(&durationD, &durationS, notBefore, notAfter))
    {
        NX_DEBUG(typeid(Engine), "Certifificate has invalid duration: %1", rawDetails(x509));
        return true;
    }

    using namespace std::chrono;
    if (durationD * hours(24) + seconds(durationS) > seconds(kCertMaxDurationS))
    {
        NX_DEBUG(typeid(Engine), "Certifificate has incorrect duration %1d %2s > %3s: %4",
            durationD, durationS, kCertMaxDurationS, rawDetails(x509));
        return true;
    }

    NX_DEBUG(typeid(Engine), "Valid certificicate duration %1d %2s (not before %3, not after %4): %5",
        durationD, durationS, toString(notBefore), toString(notAfter), rawDetails(x509));
    return false;
}

String Engine::makeCertificateAndKey(const CertDetails& details)
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
        || !X509_set_version(x509.get(), 2) //< x509.v3
        || !ASN1_INTEGER_set(X509_get_serialNumber(x509.get()), serialNumber)
        || !X509_gmtime_adj(X509_get_notBefore(x509.get()), 0)
        || !X509_gmtime_adj(X509_get_notAfter(x509.get()), kCertMaxDurationS)
        || !X509_set_pubkey(x509.get(), pkey.get()))
    {
        NX_WARNING(typeid(Engine), "Unable to generate X509 cert");
        return String();
    }

    // Apple requirement for TLS server certificates in iOS 13 and macOS 10.15:
    // TLS server certificates must contain an ExtendedKeyUsage (EKU) extension containing
    // the id-kp-serverAuth OID.
    X509V3_CTX ctx;
    X509V3_set_ctx_nodb(&ctx);
    X509V3_set_ctx(&ctx, x509.get(), x509.get(), nullptr, nullptr, 0);

    const auto addExt =
        [&ctx, &x509](int nid, const char* value) -> bool
        {
            auto ex = utils::wrapUnique(
                X509V3_EXT_conf_nid(nullptr, &ctx, nid, const_cast<char*>(value)),
                &X509_EXTENSION_free);
            return ex && X509_add_ext(x509.get(), ex.get(), -1);
        };

    static const auto kKeyUsage =
        "digitalSignature,nonRepudiation,keyEncipherment,dataEncipherment,keyAgreement,keyCertSign";

    if (!setDetails(x509.get(), details)
        || !addExt(NID_key_usage, kKeyUsage)
        || !addExt(NID_ext_key_usage, "serverAuth")
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

static bool x509load(
    ssl::SslStaticData* sslData, const Buffer& certBytes, const Engine::CertDetails& ownDetails)
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

    if (isSelfSigned(x509.get(), ownDetails) && isExpired(x509.get()))
    {
        NX_WARNING(typeid(Engine), "Reject to use expired self-signed primary X509: %1",
            rawDetails(x509.get()));
        return false;
    }

    size_t chainSize = (size_t)certSize;
    if (!SSL_CTX_use_certificate(sslData->serverContext(), x509.get()))
    {
        NX_WARNING(typeid(Engine), "Unable to use primary X509: %1", rawDetails(x509.get()));
        return false;
    }

    NX_INFO(typeid(Engine), "Primary X509 is loaded: %1", rawDetails(x509.get()));
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
            NX_INFO(typeid(Engine), "Chained X509 is loaded: %1", rawDetails(x509.get()));
            x509.release();
        }
        else
        {
            NX_WARNING(typeid(Engine), "Unable to load chained X509: %1", rawDetails(x509.get()));
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

bool Engine::useCertificateAndPkey(const String& certData, const CertDetails& ownDetails)
{
    const auto sslData = ssl::SslStaticData::instance();
    return x509load(sslData, certData, ownDetails) && pKeyLoad(sslData, certData);
}

bool Engine::useOrCreateCertificate(const QString& filePath, const CertDetails& ownDetails)
{
    if (loadCertificateFromFile(filePath, ownDetails))
        return true;

    NX_INFO(typeid(Engine), "Unable to find valid SSL certificate '%1', generate new one",
        filePath);

    const auto certData = makeCertificateAndKey(ownDetails);

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

    return useCertificateAndPkey(certData, ownDetails);
}

bool Engine::loadCertificateFromFile(const QString& filePath, const CertDetails& ownDetails)
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
    if (!useCertificateAndPkey(certData, ownDetails))
    {
        NX_INFO(typeid(Engine), "Failed to load certificate from '%1'", filePath);
        return false;
    }

    NX_INFO(typeid(Engine), "Used certificate from '%1'", filePath);
    return true;
}

void Engine::useRandomCertificate(const String& module)
{
    const auto sslCert = makeCertificateAndKey({module, "US", "Network Optix"});
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
