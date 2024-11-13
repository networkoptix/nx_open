// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "certificate.h"

#ifdef ENABLE_SSL

#include <fstream>

#include <openssl/err.h>
#include <openssl/x509v3.h>

#include <QtCore/QDir>
#include <QtNetwork/QSslCertificate>
#include <QtNetwork/QSslConfiguration>
#include <QtNetwork/QSslKey>

#include <nx/utils/log/log.h>
#include <nx/utils/random.h>
#include <nx/utils/std_string_utils.h>
#include <nx/utils/type_utils.h>

#include "context.h"

#if defined(Q_OS_IOS)
    #include "certificate_mac.h"
#endif

namespace {

class CaStore
{
public:
    CaStore()
    {
        m_x509Store = nx::utils::wrapUnique(X509_STORE_new(), &X509_STORE_free);

        if (!NX_ASSERT(m_x509Store, "Unable to create certificate store"))
            return;

        // Fill the store by system certificates. We use QSslConfiguration here to avoid the need
        // to maintain our own list of system-specific certificate paths for different OS.
        const auto caCerts = QSslConfiguration::defaultConfiguration().caCertificates();
        for (const auto& cert: caCerts)
        {
            auto converted = nx::network::ssl::Certificate::parse(cert.toPem().data());

            if (!NX_ASSERT(converted.size() == 1))
                continue;

            X509_STORE_add_cert(m_x509Store.get(), converted.at(0).x509());
        }
    }

    X509_STORE* x509Store()
    {
        m_x509StoreUnused = false;
        return m_x509Store.get();
    }

    bool isX509StoreUnused() const
    {
        return m_x509StoreUnused.load();
    }

    static CaStore& instance()
    {
        static CaStore caStore;
        return caStore;
    }

private:
    std::unique_ptr<X509_STORE, decltype(&X509_STORE_free)> m_x509Store{nullptr, {}};
    std::atomic<bool> m_x509StoreUnused{true};
};

std::string lastError()
{
    char message[1024];
    std::string result;
    while (int error = ERR_get_error())
    {
        ERR_error_string_n(error, message, sizeof(message));
        if (!result.empty())
            result += "; ";
        result += message;
    }
    return result;
}

void setErrorAndLog(std::string* errorMessage, std::string_view message)
{
    if (errorMessage)
        *errorMessage = message;
    NX_ERROR(typeid(nx::network::ssl::Certificate), message);
}

bool validateKey(EVP_PKEY* pkey, std::string* errorMessage, bool allowEcdsaCertificates)
{
    // Fail on invalid key ciphers.
    auto keyType = EVP_PKEY_id(pkey);
    if (keyType == EVP_PKEY_EC && !allowEcdsaCertificates)
    {
        setErrorAndLog(errorMessage, "Invalid key cipher (ECDSA). Failing load.");
        return false;
    }

    // Fail on invalid RSA lengths.
    if (keyType == EVP_PKEY_RSA && RSA_bits(EVP_PKEY_get0_RSA(pkey)) == 16384)
    {
        setErrorAndLog(errorMessage, "Invalid RSA key length (16384). Failing load.");
        return false;
    }

    return true;
}

} // namespace

namespace nx::network::ssl {

static const BUF_MEM* bioBuffer(BIO* bio)
{
    BUF_MEM* mem = nullptr;
    const auto error = BIO_get_mem_ptr(bio, &mem);
    NX_ASSERT(error == 1 && mem && mem->length > 0,
        "BIO_get_mem_ptr(bio, &mem): result %1, mem: %2, mem->length: %3",
        error, mem, mem ? mem->length : 0);
    return mem;
}

static std::string bioToString(BIO* bio)
{
    auto mem = bioBuffer(bio);
    return mem ? std::string(mem->data, mem->length) : std::string();
}

static QByteArray pemByteArray(X509* x509)
{
    const auto bio = nx::utils::wrapUnique(BIO_new(BIO_s_mem()), &BIO_free);
    if (NX_ASSERT(PEM_write_bio_X509(bio.get(), x509) == 1))
    {
        if (auto mem = bioBuffer(bio.get()))
            return QByteArray(mem->data, mem->length);
    }
    return QByteArray();
}

static std::chrono::system_clock::time_point timePoint(const ASN1_TIME* asn1Time)
{
    static const auto kZeroTime =
        []()
        {
            auto zeroTime = nx::utils::wrapUnique(ASN1_TIME_new(), &ASN1_TIME_free);
            ASN1_TIME_set(zeroTime.get(), 0);
            return zeroTime;
        }();
    int days = 0;
    int seconds = 0;
    ASN1_TIME_diff(&days, &seconds, kZeroTime.get(), asn1Time);
    return std::chrono::system_clock::from_time_t((time_t) days * (24 * 60 * 60) + seconds);
}

static std::vector<uint8_t> bignumToByteArray(const BIGNUM* value)
{
    std::vector<uint8_t> result(BN_num_bytes(value));
    BN_bn2bin(value, result.data());
    return result;
}

static std::vector<uint8_t> bitStringToByteArray(const ASN1_BIT_STRING* s)
{
    return s ? std::vector<uint8_t>(s->data, s->data + s->length) : std::vector<uint8_t>();
}

static std::vector<unsigned char> fingerprint(const X509* x509, const EVP_MD *digest)
{
    std::vector<unsigned char> result;

    unsigned char md[EVP_MAX_MD_SIZE];
    unsigned int len;
    if (X509_digest(x509, digest, md, &len))
        result.assign(md, md+len);

    return result;
}

static Certificate::KeyInformation fillPublicKeyInformation(EVP_PKEY* pkey)
{
    Certificate::KeyInformation result = {EVP_PKEY_base_id(pkey), {}, {}};
    switch (result.algorithmId)
    {
        case EVP_PKEY_RSA: {
            const RSA* rsa = EVP_PKEY_get0_RSA(pkey);
            result.modulus = bignumToByteArray(RSA_get0_n(rsa));
            result.rsa = {bignumToByteArray(RSA_get0_e(rsa)), RSA_bits(rsa)};
            break;
        }
        case EVP_PKEY_DSA:
            result.modulus = bignumToByteArray(DSA_get0_pub_key(EVP_PKEY_get0_DSA(pkey)));
            break;
    }
    return result;
}

template<typename Func>
static void forEachSubjectAlternativeName(const X509* x509, Func&& f)
{
    const auto names = nx::utils::wrapUnique(
        (GENERAL_NAMES*) X509_get_ext_d2i(x509, NID_subject_alt_name, 0, 0),
        &GENERAL_NAMES_free);
    for (int i = 0; i < sk_GENERAL_NAME_num(names.get()); ++i)
    {
        GENERAL_NAME* name = sk_GENERAL_NAME_value(names.get(), i);
        f(name);
    }
}

static void fillHostsFromSubjectAlternativeNames(const X509* x509, std::set<std::string>* hosts)
{
    forEachSubjectAlternativeName(
        x509,
        [hosts](GENERAL_NAME* name)
        {
            if (name->type == GEN_DNS)
            {
                ASN1_IA5STRING* dnsName = name->d.dNSName;
                if (dnsName
                    && dnsName->type == V_ASN1_IA5STRING
                    && dnsName->data
                    && dnsName->length > 0)
                {
                    hosts->insert(std::string((const char*) dnsName->data, dnsName->length));
                }
            }
        });
}

static void fillHostsFromLastSubjectCommonName(const X509* x509, std::set<std::string>* hosts)
{
    X509_name_st* subjectName = X509_get_subject_name(x509);
    int i = -1;
    ASN1_STRING* lastCommonName = nullptr;
    while ((i = X509_NAME_get_index_by_NID(subjectName, NID_commonName, i)) >= 0)
        lastCommonName = X509_NAME_ENTRY_get_data(X509_NAME_get_entry(subjectName, i));

    if (lastCommonName && lastCommonName->data && lastCommonName->length > 0)
        hosts->insert(std::string((const char*) lastCommonName->data, lastCommonName->length));
}

static bool getSubjectAltNamesText(const X509* x509, std::string* result)
{
    bool isOk = true;
    const auto bio = nx::utils::wrapUnique(BIO_new(BIO_s_mem()), &BIO_free);
    forEachSubjectAlternativeName(x509, [&bio, &isOk](GENERAL_NAME* name)
        {
            if (!isOk)
                return;
            const char comma[2] = {',', ' '};
            if (BIO_write(bio.get(), comma, sizeof(comma)) != sizeof(comma))
            {
                isOk = false;
                return;
            }
            if (GENERAL_NAME_print(bio.get(), name) <= 0)
            {
                isOk = false;
                return;
            }
        });

    if (!isOk)
        return false;

    const BUF_MEM* buf = bioBuffer(bio.get());
    if (!buf)
        return false;

    if (buf->length < 3)
    {
        *result = "";
        return true;
    }

    // Remove leading comma and tailing '\n'.
    *result = std::string(buf->data + 2, buf->length - 3);
    return true;
}

X509Name::X509Name(const X509_name_st* name)
{
    const int nameEntryCount = X509_NAME_entry_count(name);
    for (int i = 0; i < nameEntryCount; ++i)
    {
        auto entry = X509_NAME_get_entry(name, i);
        if (!entry)
            continue;
        auto entryObject = X509_NAME_ENTRY_get_object(entry);
        if (!entryObject)
            continue;
        auto entryData = X509_NAME_ENTRY_get_data(entry);
        if (!entryData || !entryData->data || entryData->length <= 0)
            continue;

        attrs[OBJ_nid2sn(OBJ_obj2nid(entryObject))] =
            std::string((const char*) entryData->data, entryData->length);
    }
}

bool operator==(const CertificateView& lhs, const CertificateView& rhs)
{
    return X509_cmp(lhs.x509(), rhs.x509()) == 0;
}

CertificateView::CertificateView(X509* x509): m_x509(x509)
{
}

X509Name CertificateView::issuer() const
{
    return X509_get_issuer_name(x509());
}

long CertificateView::serialNumber() const
{
    return ASN1_INTEGER_get(X509_get0_serialNumber(x509()));
}

std::chrono::system_clock::time_point CertificateView::notBefore() const
{
    return timePoint(X509_get0_notBefore(x509()));
}

std::chrono::system_clock::time_point CertificateView::notAfter() const
{
    return timePoint(X509_get0_notAfter(x509()));
}

int CertificateView::version() const
{
    return X509_get_version(x509());
}

X509Name CertificateView::subject() const
{
    return X509_get_subject_name(x509());
}

std::vector<uint8_t> CertificateView::signature() const
{
    const ASN1_BIT_STRING* s = nullptr;
    X509_get0_signature(&s, nullptr, x509());
    return bitStringToByteArray(s);
}

const char* CertificateView::signatureAlgorithm() const
{
    return OBJ_nid2ln(X509_get_signature_nid(x509()));
}

std::vector<CertificateView::Extension> CertificateView::extensions() const
{
    std::vector<CertificateView::Extension> result;
    const int count = X509_get_ext_count(x509());
    for (int i = 0; i < count; ++i)
    {
        X509_EXTENSION* extention = X509_get_ext(x509(), i);
        const X509V3_EXT_METHOD* method = X509V3_EXT_get(extention);
        const auto bio = nx::utils::wrapUnique(BIO_new(BIO_s_mem()), &BIO_free);
        X509V3_EXT_print(bio.get(), extention, /*flag*/ 0, /*indent*/ 0);
        const int critical = X509_EXTENSION_get_critical(extention);
        result.push_back({method ? method->ext_nid : NID_undef, bioToString(bio.get()), critical != 0});
    }
    return result;
}

std::set<std::string> CertificateView::hosts() const
{
    std::set<std::string> result;
    fillHostsFromSubjectAlternativeNames(m_x509, &result);
    if (result.empty())
        fillHostsFromLastSubjectCommonName(m_x509, &result);
    return result;
}

std::string CertificateView::subjectAltNames() const
{
    std::string result;
    if (!getSubjectAltNamesText(m_x509, &result))
        return "";
    return result;
}

std::string CertificateView::publicKey() const
{
    EVP_PKEY* pubKey = nullptr;
    if (!NX_ASSERT(pubKey = X509_get0_pubkey(x509())))
        return {};
    const auto bio = nx::utils::wrapUnique(BIO_new(BIO_s_mem()), BIO_free);
    if (!NX_ASSERT(PEM_write_bio_PUBKEY(bio.get(), pubKey) == 1))
        return {};
    return bioToString(bio.get());
}

CertificateView::KeyInformation CertificateView::publicKeyInformation() const
{
    EVP_PKEY* pkey = nullptr;
    if (!NX_ASSERT(pkey = X509_get0_pubkey(x509())))
        return {};
    return fillPublicKeyInformation(pkey);
}

std::vector<unsigned char> CertificateView::sha1() const
{
    const auto digest = EVP_sha1();
    if (NX_ASSERT(digest))
        return fingerprint(x509(), digest);
    return {};
}

std::vector<unsigned char> CertificateView::sha256() const
{
    const auto digest = EVP_sha256();
    if (NX_ASSERT(digest))
        return fingerprint(x509(), digest);
    return {};
}

std::string CertificateView::printedText() const
{
    auto output = nx::utils::wrapUnique(BIO_new(BIO_s_mem()), &BIO_free);
    X509_print_ex(output.get(), x509(), XN_FLAG_COMPAT, X509_FLAG_COMPAT);
    return bioToString(output.get());
}

bool CertificateView::KeyInformation::operator==(const CertificateView::KeyInformation& rhs) const
{
    if (algorithmId == rhs.algorithmId
        && modulus == rhs.modulus
        && rsa.has_value() == rhs.rsa.has_value())
    {
        return rsa->bits == rhs.rsa->bits && rsa->exponent == rhs.rsa->exponent;
    }
    return false;
}

bool CertificateView::KeyInformation::parsePem(const std::string& str)
{
    auto bio = utils::wrapUnique(
        BIO_new_mem_buf(const_cast<void*>(static_cast<const void*>(str.c_str())), str.size()),
        BIO_free);
    if (!NX_ASSERT(!!bio))
        return false;

    auto pkey = utils::wrapUnique(
        PEM_read_bio_PUBKEY(bio.get(), nullptr, nullptr, nullptr),
        EVP_PKEY_free);
    if (!pkey)
    {
        NX_DEBUG(this, "Unable to parse public key from PEM");
        return false;
    }
    *this = fillPublicKeyInformation(pkey.get());
    return true;
}

Certificate::Certificate(X509* x509)
    :
    CertificateView(X509_dup(x509)),
    m_x509(CertificateView::x509(), &X509_free)
{
}

Certificate::Certificate(const CertificateView& view)
    :
    CertificateView(X509_dup(view.x509())),
    m_x509(CertificateView::x509(), &X509_free)
{
}

std::vector<Certificate> Certificate::parse(const std::string& pemString, bool assertOnFail)
{
    X509Certificate x509;

    bool parsed = !pemString.empty() && x509.parsePem(pemString);
    if (parsed)
        return x509.certificates();

    // Report an error.
    const auto errorMessage = "Invalid certificate passed\n" + pemString;
    if (assertOnFail)
        NX_ASSERT(parsed, errorMessage);
    else
        NX_VERBOSE(NX_SCOPE_TAG, errorMessage);

    return {};
}

X509Certificate::X509Certificate(X509* x509):
    m_x509(X509_dup(x509), &X509_free)
{
}

X509Certificate::X509Certificate(const X509Certificate& certificate):
    m_x509(X509_dup(certificate.m_x509.get()), &X509_free)
{
    for (const auto& i: certificate.m_extraChainCerts)
        m_extraChainCerts.emplace_back(X509Ptr(X509_dup(i.get()), &X509_free));
}

X509Certificate& X509Certificate::operator=(const X509Certificate& certificate)
{
    m_x509.reset(X509_dup(certificate.m_x509.get()));

    m_extraChainCerts.clear();
    for (const auto& i: certificate.m_extraChainCerts)
        m_extraChainCerts.emplace_back(X509Ptr(X509_dup(i.get()), &X509_free));
    return *this;
}

bool X509Certificate::parsePem(
    const std::string& pem, std::optional<int> maxChainLength, std::string* errorMessage)
{
    auto bio = utils::wrapUnique(
        BIO_new_mem_buf(const_cast<void*>((const void*)pem.data()), pem.size()),
        &BIO_free);

    auto x509 = utils::wrapUnique(PEM_read_bio_X509_AUX(bio.get(), 0, 0, 0), &X509_free);
    auto certSize = i2d_X509(x509.get(), 0);
    if (!x509 || certSize <= 0)
    {
        if (errorMessage)
            nx::utils::buildString(errorMessage, lastError(), " on read of certificate ", pem);
        NX_DEBUG(this, "%1 on read of certificate %2", lastError(), pem);
        return false;
    }

    int chainSize = certSize;
    m_x509 = std::exchange(x509, nullptr);

    m_extraChainCerts.clear();
    for (int i = 1; ; ++i)
    {
        x509 = utils::wrapUnique(PEM_read_bio_X509_AUX(bio.get(), 0, 0, 0), &X509_free);
        certSize = i2d_X509(x509.get(), 0);
        if (!x509 || certSize <= 0)
            return true;

        NX_VERBOSE(this, "X.509 certificate from chain was loaded: %1. Chain length %2",
            toString(x509.get()), i);

        chainSize += certSize;
        if (maxChainLength && chainSize > * maxChainLength)
        {
            NX_DEBUG(this, "Certificate chain is too long: %1 vs %2", chainSize, *maxChainLength);
            return true;
        }

        m_extraChainCerts.push_back(std::exchange(x509, nullptr));
    }
}

bool X509Certificate::isValid(const std::chrono::seconds& maxDuration) const
{
    const auto notBefore = X509_get_notBefore(m_x509.get());
    if (notBefore && X509_cmp_time(notBefore, NULL) > 0)
    {
        NX_DEBUG(this, "Certificate is from the future %1", toString(notBefore));
        return false;
    }

    const auto notAfter = X509_get_notAfter(m_x509.get());
    if (notAfter && X509_cmp_time(notAfter, NULL) < 0)
    {
        NX_DEBUG(this, "Certificate is expired %1", toString(notAfter));
        return false;
    }

    const auto durationValue = duration();
    if (!durationValue || (*durationValue > maxDuration))
    {
        NX_DEBUG(this, "Certificate duration %1 is not allowed", durationValue);
        return false;
    }

    NX_VERBOSE(this, "Certificate is valid (from %1, to %2)",
        toString(notBefore), toString(notAfter));
    return true;
}

std::optional<std::chrono::seconds> X509Certificate::duration() const
{
    const auto notBefore = X509_get_notBefore(m_x509.get());
    const auto notAfter = X509_get_notAfter(m_x509.get());

    int durationD = 0, durationS = 0;
    if (!ASN1_TIME_diff(&durationD, &durationS, notBefore, notAfter))
    {
        NX_DEBUG(this, "Certificate has invalid duration");
        return std::nullopt;
    }

    const auto validPeriod = durationD * std::chrono::hours(24) + std::chrono::seconds(durationS);
    NX_VERBOSE(this, "Certificate has duration %1", validPeriod);
    return validPeriod;
}

bool X509Certificate::isSignedBy(const X509Name& alleged) const
{
    return alleged == X509_get_issuer_name(m_x509.get());
}

bool X509Certificate::bindToContext(SSL_CTX* sslContext, std::string* errorMessage) const
{
    if (!SSL_CTX_use_certificate(sslContext, m_x509.get()))
    {
        if (errorMessage)
        {
            nx::utils::buildString(errorMessage,
                lastError(), " on binding to SSL context of certificate ", toString());
        }
        NX_DEBUG(this, "%1 on binding to SSL context of certificate %2", lastError(), *this);
        return false;
    }

    for (auto& x509: m_extraChainCerts)
    {
        if (!SSL_CTX_add_extra_chain_cert(sslContext, x509.get()))
        {
            if (errorMessage)
            {
                nx::utils::buildString(errorMessage,
                    lastError(), " on loading of chained X.509: ", toString(x509.get()));
            }
            NX_DEBUG(this, "%1 on loading of chained X.509: %2", lastError(), toString(x509.get()));
            return false;
        }

        // SSL_CTX_add_extra_chain_cert assumes ownership of the X509 but it does not
        // increment its ref count. That's different from SSL_CTX_use_certificate which
        // invokes X509_up_ref(X509*) itself.
        X509_up_ref(x509.get());
    }

    return true;
}

X509* X509Certificate::x509() const
{
    return m_x509.get();
}

std::string X509Certificate::toString() const
{
    return toString(m_x509.get());
}

std::string X509Certificate::idForToStringFromPtr() const
{
    return toString();
}

std::string X509Certificate::toString(const X509* x509)
{
    if (!x509)
        return "null";

    auto subject = utils::wrapUnique(X509_NAME_oneline(
        X509_get_subject_name(x509), NULL, 0), [](char* ptr) { free(ptr); });

    if (!subject || !subject.get())
        return std::string("Unable to read certificate subject");

    std::string info(subject.get() + 1);
    return nx::utils::replace(info, "/", ", ");
}

std::string X509Certificate::toString(const ASN1_TIME* time)
{
    const auto bio = nx::utils::wrapUnique(BIO_new(BIO_s_mem()), &BIO_free);
    if (!NX_ASSERT(ASN1_TIME_print(bio.get(), time) == 1))
        return {};
    return bioToString(bio.get());
}

std::string X509Certificate::pemString() const
{
    std::string result;
    const auto bio = nx::utils::wrapUnique(BIO_new(BIO_s_mem()), &BIO_free);
    if (!NX_ASSERT(PEM_write_bio_X509(bio.get(), m_x509.get()) == 1))
        return result;
    result = bioToString(bio.get());
    for (const auto& x509: m_extraChainCerts)
    {
        const auto bio = nx::utils::wrapUnique(BIO_new(BIO_s_mem()), &BIO_free);
        if (!NX_ASSERT(PEM_write_bio_X509(bio.get(), x509.get()) == 1))
            break;
        result += bioToString(bio.get());
    }
    return result;
}

std::vector<Certificate> X509Certificate::certificates() const
{
    std::vector<Certificate> result;
    result.push_back(m_x509.get());
    for (const auto& x509: m_extraChainCerts)
        result.push_back(x509.get());
    return result;
}

std::set<std::string> X509Certificate::hosts() const
{
    std::set<std::string> result;
    fillHostsFromSubjectAlternativeNames(m_x509.get(), &result);
    if (result.empty())
        fillHostsFromLastSubjectCommonName(m_x509.get(), &result);
    for (const auto& extra: m_extraChainCerts)
        fillHostsFromSubjectAlternativeNames(extra.get(), &result);
    return result;
}

std::string X509Certificate::subjectAltNames() const
{
    std::string result;
    if (!getSubjectAltNamesText(m_x509.get(), &result))
        return "";
    return result;
}

//-------------------------------------------------------------------------------------------------

Pem::Pem():
    m_pkey(nullptr, &EVP_PKEY_free)
{
}

/* Note that EVP_PKEY_dup() is introduced in OpenSSL 3.0
 * Now this deep copy uses EVP_PKEY_up_ref(), which is probably unsafe
 * */

Pem::Pem(const Pem& pem):
    m_certificate(pem.m_certificate),
    m_pkey(EVP_PKEY_up_ref(pem.m_pkey.get()) != 0 ? pem.m_pkey.get() : nullptr, &EVP_PKEY_free)
{
}

Pem& Pem::operator=(const Pem& pem)
{
    m_certificate = pem.m_certificate;
    m_pkey.reset(EVP_PKEY_up_ref(pem.m_pkey.get()) != 0 ? pem.m_pkey.get() : nullptr);
    return *this;
}

bool Pem::parse(const std::string& str, std::string* errorMessage, bool allowEcdsaCertificates)
{
    return m_certificate.parsePem(str, /*maxChainLength*/ std::nullopt, errorMessage)
        && loadPrivateKey(str, errorMessage, allowEcdsaCertificates);
}

bool Pem::bindToContext(SSL_CTX* sslContext, std::string* errorMessage) const
{
    if (!m_certificate.bindToContext(sslContext, errorMessage))
        return false;

    if (!SSL_CTX_use_PrivateKey(sslContext, m_pkey.get()))
    {
        if (errorMessage)
        {
            nx::utils::buildString(errorMessage,
                lastError(), " on using of private key of certificate ", m_certificate.toString());
        }
        NX_DEBUG(this, "%1 on using of private key of certificate %2", lastError(), m_certificate);
        return false;
    }

    return true;
}

std::string Pem::toString() const
{
    return m_certificate.toString();
}

X509Certificate& Pem::certificate()
{
    return m_certificate;
}

const X509Certificate& Pem::certificate() const
{
    return m_certificate;
}

bool Pem::loadPrivateKey(const std::string& pem, std::string* errorMessage, bool allowEcdsaCertificates)
{
    auto bio = utils::wrapUnique(
        BIO_new_mem_buf(pem.data(), (int) pem.size()),
        &BIO_free);
    if (!bio)
    {
        setErrorAndLog(errorMessage, "Failed to create BIO for the private key.");
        return false;
    }

    auto pkey = utils::wrapUnique(
        PEM_read_bio_PrivateKey(bio.get(), nullptr, nullptr, nullptr),
        &EVP_PKEY_free);
    if (!pkey)
    {
        setErrorAndLog(errorMessage, "Failed to read private key from PEM.");
        return false;
    }

    if (!validateKey(pkey.get(), errorMessage, allowEcdsaCertificates))
        return false;

    m_pkey = std::move(pkey);
    NX_VERBOSE(this, "PKEY is loaded (SSL init is complete)");
    return true;
}

//-------------------------------------------------------------------------------------------------

static bool setIssuerOrSubject(X509* x509, const X509Name& issuerAndSubject, bool isIssuer)
{
    const auto name = utils::wrapUnique(X509_NAME_new(), &X509_NAME_free);
    if (!NX_ASSERT(name, "Failed to create new X509_NAME"))
        return false;

    const auto setField =
        [name = name.get()](const std::string& field, const std::string& value)
        {
            return X509_NAME_add_entry_by_txt(
                name,
                field.c_str(),
                MBSTRING_UTF8,
                (unsigned char *) value.data(),
                value.size(),
                -1,
                0);
        };

    for (const auto& [name, value]: issuerAndSubject.attrs)
    {
        if (!NX_ASSERT(
            setField(name, value), "Failed to set X509_NAME %1 with value %2", name, value))
        {
            return false;
        }
    }

    if (isIssuer)
    {
        if (!NX_ASSERT(X509_set_issuer_name(x509, name.get())))
            return false;
    }
    else
    {
        if (!NX_ASSERT(X509_set_subject_name(x509, name.get())))
            return false;
    }

    return true;
}

static bool setDnsName(X509* x509, const std::string& dnsName)
{
    auto names = utils::wrapUnique(sk_GENERAL_NAME_new_null(), &GENERAL_NAMES_free);
    if (!NX_ASSERT(names, "Failed to create new GENERAL_NAMES"))
        return false;

    enum class EntryType
    {
        dns,
        ip
    };
    auto parseEntry = [](const std::string_view& str) -> std::tuple<EntryType, std::string_view>
    {
        auto pos = str.find(':');
        if (pos == std::string_view::npos)
            return {EntryType::dns, str};
        std::string_view prefix(str.data(), pos);
        std::string_view name(str.data() + pos + 1, str.size() - pos - 1);
        if (prefix == "IP Address")
            return {EntryType::ip, name};
        return {EntryType::dns, name};
    };

    std::string::const_iterator begin = dnsName.begin();
    std::string::const_iterator end;
    do
    {
        end = std::find(begin, dnsName.end(), ',');

        std::string oneName(begin, end);
        auto [type, nameEntry] = parseEntry(oneName);

        auto name = utils::wrapUnique(GENERAL_NAME_new(), &GENERAL_NAME_free);
        if (!NX_ASSERT(name, "Failed to create new GENERAL_NAME"))
            return false;

        if (type == EntryType::dns)
        {
            auto ia5String = utils::wrapUnique(ASN1_IA5STRING_new(), &ASN1_STRING_free);
            if (!NX_ASSERT(ia5String, "Failed to create new ASN1_IA5STRING"))
                return false;

            if (!NX_ASSERT(ASN1_STRING_set(ia5String.get(), nameEntry.data(), nameEntry.length())))
                return false;

            GENERAL_NAME_set0_value(name.get(), GEN_DNS, ia5String.release());
        }
        else
        {
            auto octetString = utils::wrapUnique(
                ASN1_OCTET_STRING_new(),
                &ASN1_OCTET_STRING_free);
            if (!NX_ASSERT(octetString, "Failed to create new ASN1_OCTET_STRING"))
                return false;

            if (auto addrV4 = nx::network::HostAddress::ipV4from(nameEntry))
            {
                const auto* bytes = reinterpret_cast<const uint8_t*>(&addrV4->s_addr);
                const size_t size = sizeof(addrV4->s_addr);

                if (!NX_ASSERT(ASN1_STRING_set(octetString.get(), bytes, size)))
                    return false;

                GENERAL_NAME_set0_value(name.get(), GEN_IPADD, octetString.release());
            }
            else if (auto addrV6 = nx::network::HostAddress::ipV6from(nameEntry); addrV6.first)
            {
                const auto* bytes = addrV6.first->s6_addr;
                const size_t size = sizeof(addrV6.first->s6_addr);

                if (!NX_ASSERT(ASN1_STRING_set(octetString.get(), bytes, size)))
                    return false;

                GENERAL_NAME_set0_value(name.get(), GEN_IPADD, octetString.release());
            }
            else
            {
                NX_ASSERT(false, "Invalid IP address format: %1", nameEntry);
                return false;
            }
        }

        if (!NX_ASSERT(sk_GENERAL_NAME_push(names.get(), name.release())))
            return false;

        begin = end;

        while (begin < dnsName.end() && (*begin == ',' || *begin == ' '))
            ++begin;
    } while (end != dnsName.end());

    if (!NX_ASSERT(X509_add1_ext_i2d(x509, NID_subject_alt_name, names.get(), 0, 0)))
        return false;

    return true;
}

PKeyPtr generateKey(int length /*= kRsaLength*/)
{
    PKeyPtr nullValue = PKeyPtr(nullptr, [](EVP_PKEY*) {});

    auto number = utils::wrapUnique(BN_new(), &BN_free);
    if (!number || !BN_set_word(number.get(), RSA_F4))
    {
        NX_WARNING(typeid(Certificate), "Unable to generate big number");
        return nullValue;
    }

    auto rsa = utils::wrapUnique(RSA_new(), &RSA_free);
    if (!rsa || !RSA_generate_key_ex(rsa.get(), length, number.get(), NULL))
    {
        NX_WARNING(typeid(Certificate), "Unable to generate RSA");
        return nullValue;
    }

    auto pkey = utils::wrapUnique(EVP_PKEY_new(), &EVP_PKEY_free);
    if (!pkey || !EVP_PKEY_assign_RSA(pkey.get(), rsa.release()))
    {
        NX_WARNING(typeid(Certificate), "Unable to generate PKEY");
        return nullValue;
    }

    return pkey;
}

bool generateKeyFiles(
    const std::string& privateKeyFileName,
    const std::string& publicKeyFileName,
    int rsaKeyLength /*= kRsaLength*/)
{
    auto key = generateKey(rsaKeyLength);

    FILE* f = fopen(publicKeyFileName.c_str(), "wt");
    if (!f)
    {
        NX_WARNING(typeid(Certificate), "Cannot open file: %1", publicKeyFileName);
        return false;
    }
    if (PEM_write_PUBKEY(f, key.get()) != 1)
    {
        fclose(f);
        NX_WARNING(typeid(Certificate), "PEM_write_PUBKEY() failed: %1", publicKeyFileName);
        return false;
    }
    if (fclose(f) != 0)
    {
        NX_WARNING(typeid(Certificate), "fclose() failed: %1", publicKeyFileName);
        return false;
    }

    f = fopen(privateKeyFileName.c_str(), "wt");
    if (!f)
    {
        NX_WARNING(typeid(Certificate), "Cannot open file: %1", privateKeyFileName);
        return false;
    }
    if (PEM_write_PrivateKey(
        f,
        key.get(),
        /* default cipher for encrypting the key on disk */ nullptr,
        /* passphrase required for decrypting the key on disk */ nullptr,
        /* length of the passphrase string */ 0,
        /* callback for requesting a password */ nullptr,
        /* data to pass to the callback */ nullptr) != 1)
    {
        fclose(f);
        NX_WARNING(typeid(Certificate), "PEM_write_PrivateKey() failed: %1", privateKeyFileName);
        return false;
    }
    if (fclose(f) != 0)
    {
        NX_WARNING(typeid(Certificate), "fclose() failed: %1", privateKeyFileName);
        return false;
    }
    return true;
}

std::string makeCertificateAndKey(
    const X509Name& issuerAndSubject,
    const std::string& dnsName,
    std::optional<long> serialNumber,
    std::chrono::seconds notBeforeAdjust,
    std::chrono::seconds notAfterAdjust)
{
    // TODO: remove ssl::Context::instance() from here.
    ssl::Context::instance();

    if (!serialNumber)
        serialNumber = nx::utils::random::number<long>();

    const auto pkey = generateKey();
    if (!pkey)
    {
        return std::string();
    }

    return makeCertificate(
        pkey,
        issuerAndSubject,
        dnsName,
        std::move(serialNumber),
        std::move(notBeforeAdjust),
        std::move(notAfterAdjust));
}

bool makeCertificateAndKeyFile(
    const std::string& filePath,
    const X509Name& issuerAndSubject,
    const std::string& hostName,
    std::optional<long> serialNumber,
    std::chrono::seconds notBeforeAdjust,
    std::chrono::seconds notAfterAdjust)
{
    std::string pem = makeCertificateAndKey(
            issuerAndSubject,
            hostName,
            std::move(serialNumber),
            notBeforeAdjust,
            notAfterAdjust);
    if (pem.empty())
        return false;

    std::ofstream certFile(filePath, std::ios::trunc);
    if (!certFile)
        return false;
    certFile << pem;
    certFile.flush();
    certFile.close();
    return !certFile.fail();
}

std::string makeCertificate(
    const PKeyPtr& pkey,
    const X509Name& issuerAndSubject,
    const std::string& dnsName,
    std::optional<long> serialNumber,
    std::chrono::seconds notBeforeAdjust,
    std::chrono::seconds notAfterAdjust)
{
    return makeCertificate(
        pkey,
        pkey,
        issuerAndSubject,
        std::nullopt,
        dnsName,
        serialNumber,
        notBeforeAdjust,
        notAfterAdjust,
        std::nullopt);
}

std::string makeCertificate(
    const PKeyPtr& keyPair,
    const PKeyPtr& signingKey,
    const X509Name& subject,
    std::optional<CertificateView> issuerCertificate,
    const std::string& hostName,
    std::optional<long> serialNumber /*= std::nullopt*/,
    std::chrono::seconds notBeforeAdjust /*= std::chrono::seconds::zero()*/,
    std::chrono::seconds notAfterAdjust /*= kCertMaxDuration*/,
    std::optional<bool> isCa /*= std::nullopt*/)
{
    // TODO: remove ssl::Context::instance() from here.
    ssl::Context::instance();

    if (!serialNumber)
        serialNumber = nx::utils::random::number<long>();

    const time_t now = time(nullptr);
    auto x509 = utils::wrapUnique(X509_new(), &X509_free);
    if (!x509
        || !X509_set_version(x509.get(), 2) //< x509.v3
        || !ASN1_INTEGER_set(X509_get_serialNumber(x509.get()), *serialNumber)
        || !ASN1_TIME_adj(
            X509_get_notBefore(x509.get()), now, /*offset_day*/ 0, notBeforeAdjust.count())
        || !ASN1_TIME_adj(
            X509_get_notAfter(x509.get()), now, /*offset_day*/ 0, notAfterAdjust.count())
        || !X509_set_pubkey(x509.get(), keyPair.get()))
    {
        NX_ASSERT(false, "Unable to generate X509 cert");
        return "";
    }

    const bool isSelfSigned = !issuerCertificate.has_value();

    // Apple requirement for TLS server certificates in iOS 13 and macOS 10.15:
    // TLS server certificates must contain an ExtendedKeyUsage (EKU) extension containing
    // the id-kp-serverAuth OID.
    X509V3_CTX ctx;
    X509V3_set_ctx_nodb(&ctx);
    X509V3_set_ctx(
        &ctx,
        isSelfSigned ? x509.get() : issuerCertificate->x509(),
        x509.get(),
        nullptr,
        nullptr,
        0);

    const auto addExt =
        [&ctx, &x509](int nid, const char* value) -> bool
    {
        auto ex = utils::wrapUnique(
            X509V3_EXT_conf_nid(nullptr, &ctx, nid, const_cast<char*>(value)),
            &X509_EXTENSION_free);
        if (ex && X509_add_ext(x509.get(), ex.get(), -1))
            return true;

        NX_ASSERT(false,
            "Failed to add %1 extension with value %2 to X509 certificate",
            OBJ_nid2ln(nid),
            value);
        return false;
    };

    if (!setIssuerOrSubject(
            x509.get(),
            isSelfSigned ? subject : issuerCertificate->subject(),
            true)
        || !setIssuerOrSubject(x509.get(), subject, false)
        || (!hostName.empty() && !setDnsName(x509.get(), hostName)))
        return "";

    if (!isCa.has_value())
    {
        static const auto kKeyUsage =
            "digitalSignature,nonRepudiation,keyEncipherment,dataEncipherment,keyAgreement,"
            "keyCertSign";
        if (!addExt(NID_key_usage, kKeyUsage)
            || !addExt(NID_ext_key_usage, "serverAuth"))
            return "";
    }
    else
    {
        if (!addExt(NID_subject_key_identifier, "hash"))
            return "";

        if (!isSelfSigned)
            if (!addExt(NID_authority_key_identifier, "keyid:always"))
                return "";

        if (*isCa)
        {
            if (!addExt(NID_key_usage, "critical,keyCertSign,cRLSign")
                || !addExt(NID_basic_constraints, "critical,CA:TRUE"))
                return "";
        }
        else
        {
            if (!addExt(NID_key_usage, "critical,digitalSignature,keyEncipherment")
                || !addExt(NID_ext_key_usage, "serverAuth,clientAuth")
                || !addExt(NID_basic_constraints, "critical,CA:FALSE"))
                return "";
        }
    }

    if (!X509_sign(x509.get(), signingKey.get(), EVP_sha256()))
    {
        NX_ASSERT(false, "Failed to sign X509 certificate");
        return "";
    }

    std::string result;
    const auto bio = utils::wrapUnique(BIO_new(BIO_s_mem()), BIO_free);
    if (!bio
        || !PEM_write_bio_X509(bio.get(), x509.get())
        || !PEM_write_bio_PrivateKey(bio.get(), keyPair.get(), 0, 0, 0, 0, 0)
        || (result = bioToString(bio.get())).empty())
    {
        NX_WARNING(typeid(Certificate), "Failed to generate certificate PEM string");
    }

    return result;
}

bool useCertificate(
    const Pem& pem, const X509Name& issuer, const std::chrono::seconds& maxDuration)
{
    return pem.certificate().isSignedBy(issuer)
        && pem.certificate().isValid(maxDuration)
        && ssl::Context::instance()->setDefaultCertificate(pem);
}

bool useOrCreateCertificate(
    const std::string& filePath,
    const X509Name& issuer,
    const std::string& hostName,
    const std::chrono::seconds& maxDuration,
    std::string* certificate /*= nullptr*/)
{
    if (auto pem = readPemFile(filePath); pem && useCertificate(*pem, issuer, maxDuration))
    {
        if (certificate)
            *certificate = pem->toString();
        return true;
    }

    NX_INFO(typeid(Certificate), "Unable to load valid SSL certificate from file '%1'. "
        "Generating a new one", filePath);

    const auto certData = makeCertificateAndKey(
        issuer,
        hostName,
        /*serialNumber*/ std::nullopt,
        /*notBeforeAdjust*/ std::chrono::seconds::zero(),
        /*notAfterAdjust*/ maxDuration);

    NX_ASSERT(!certData.empty());
    if (!filePath.empty())
    {
        // TODO: Switch to std::filesystem here.
        QFileInfo(QString::fromStdString(filePath)).absoluteDir().mkpath(".");
        QFile file(QString::fromStdString(filePath));
        if (!file.open(QIODevice::WriteOnly) || file.write(certData.data(), certData.size()) != (int) certData.size())
        {
            NX_ERROR(typeid(Certificate), "Unable to write SSL certificate to file %1: %2",
                filePath, file.errorString());
        }
    }

    if (Context::instance()->setDefaultCertificate(certData))
    {
        if (certificate)
            *certificate = certData;
        return true;
    }
    return false;
}

bool loadCertificateFromFile(const std::string& filePath)
{
    if (filePath.empty())
    {
        NX_INFO(typeid(Certificate), "Certificate path is empty");
        return false;
    }

    QFile file(QString::fromStdString(filePath));
    if (!file.open(QIODevice::ReadOnly))
    {
        NX_INFO(typeid(Certificate), "Failed to open certificate file '%1': %2",
            filePath, file.errorString());
        return false;
    }
    auto certData = file.readAll().toStdString();

    NX_INFO(typeid(Certificate), "Loaded certificate from '%1'", filePath);
    if (!Context::instance()->setDefaultCertificate(certData))
    {
        NX_INFO(typeid(Certificate), "Failed to load certificate from '%1'", filePath);
        return false;
    }

    NX_INFO(typeid(Certificate), "Used certificate from '%1'", filePath);
    return true;
}

void useRandomCertificate(const std::string& module_, const std::string& hostName)
{
    const X509Name issuerAndSubject(module_, "US", "Example");
    const auto sslCert = makeCertificateAndKey(issuerAndSubject, hostName);
    NX_CRITICAL(!sslCert.empty());
    NX_CRITICAL(Context::instance()->setDefaultCertificate(sslCert));
}

std::optional<Pem> readPemFile(const std::string& filePath)
{
    if (filePath.empty())
    {
        NX_INFO(typeid(Certificate), "Certificate path is empty");
        return std::nullopt;
    }

    QFile file(QString::fromStdString(filePath));
    if (!file.open(QIODevice::ReadOnly))
    {
        NX_INFO(typeid(Certificate), "Failed to open certificate file '%1': %2",
            filePath, file.errorString());
        return std::nullopt;
    }
    auto pemStr = file.readAll().toStdString();

    Pem pem;
    if (!pem.parse(pemStr))
    {
        NX_INFO(typeid(Certificate), "Failed to parse certificate from file '%1'", filePath);
        return std::nullopt;
    }

    NX_INFO(typeid(Certificate), "Loaded certificate from '%1'", filePath);
    return pem;
}

bool verifyBySystemCertificates(
    STACK_OF(X509)* chain, const std::string& hostName, std::string* outErrorMessage)
{
    #if defined(Q_OS_IOS)
        // QSslCertificate::verify does not work properly in iOS because iOS does not provide any
        // API to extract trusted certificates from the OS, but they are required for Qt
        // implementation of verification. We use a dedicated implementation for this case which
        // uses a system API to verify the chain without extracting the certificate. Unfortunately
        // that API does not provide a way to temporarily add a trusted root certificate. It can
        // only be added to the system keychain, and we don't want that. Easy solution is to verify
        // the chain by the system certificates and if the attempt failed, retry with Qt
        // implementation which will use only the certificates we loaded explicitly (because it
        // does not have access to the system certificates).
        if (detail::verifyBySystemCertificatesMac(chain, hostName, outErrorMessage))
            return true;
    #endif

    if (outErrorMessage)
        outErrorMessage->clear();

    std::string trimmedHost = hostName;
    while (trimmedHost.ends_with('.'))
        trimmedHost.pop_back();

    const int chainSize = sk_X509_num(chain);
    QList<QSslCertificate> certificates;
    for (int i = 0; i < chainSize; ++i)
        certificates << QSslCertificate(pemByteArray(sk_X509_value(chain, i)));
    const auto errorList =
        QSslCertificate::verify(certificates, QString::fromStdString(trimmedHost));
    if (errorList.isEmpty())
        return true;

    if (outErrorMessage)
    {
        *outErrorMessage = nx::utils::buildString(
            "Verify certificate for host `",
            hostName,
            "` errors: ",
            toString(errorList).toStdString(),
            ". Chain: ",
            toString(certificates).toStdString());
    }
    return false;
}

std::vector<Certificate> completeCertificateChain(STACK_OF(X509)* chain, bool* ok)
{
    auto converted =
        [](STACK_OF(X509)* chain)
        {
            std::vector<Certificate> result;

            for (int i = 0; i < sk_X509_num(chain); ++i)
                result.push_back(Certificate(sk_X509_value(chain, i)));

            return result;
        };

    // Report a failure if we don't reach the end of this function.
    if (ok)
        *ok = false;

    auto store = CaStore::instance().x509Store();
    if (!store)
    {
        NX_WARNING(NX_SCOPE_TAG,
            "Can't complete certificate chain: OpenSSL store is not created");
        return converted(chain);
    }

    auto context = nx::utils::wrapUnique(X509_STORE_CTX_new(), &X509_STORE_CTX_free);
    if (!NX_ASSERT(context))
    {
        NX_WARNING(NX_SCOPE_TAG,
            "Can't complete certificate chain: OpenSSL context is not created");
        return converted(chain);
    }

    if (!X509_STORE_CTX_init(context.get(), store, sk_X509_value(chain, 0), chain))
    {
        NX_WARNING(NX_SCOPE_TAG,
            "Can't complete certificate chain: failed to initialize OpenSSL context");
        return converted(chain);
    }

    // Try to build certificate chain.
    X509_verify_cert(context.get());

    const auto new_chain = X509_STORE_CTX_get0_chain(context.get());
    if (!new_chain)
    {
        NX_WARNING(NX_SCOPE_TAG, "Can't complete certificate chain");
        return converted(chain);
    }

    // New chain has been built successfully.
    if (ok)
        *ok = true;

    return converted(new_chain);
}

void addTrustedRootCertificate(const CertificateView& cert)
{
    QSslConfiguration conf = QSslConfiguration::defaultConfiguration();
    conf.addCaCertificate(QSslCertificate(pemByteArray(cert.x509())));
    QSslConfiguration::setDefaultConfiguration(std::move(conf));

    NX_ASSERT(CaStore::instance().isX509StoreUnused(),
        "All trusted root certificates should be loaded on startup");
}

} // namespace nx::network::ssl

#endif // ENABLE_SSL
