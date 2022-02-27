// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "certificate.h"

#ifdef ENABLE_SSL

#include <openssl/x509v3.h>

#include <QtCore/QDir>
#include <QtNetwork/QSslCertificate>
#include <QtNetwork/QSslConfiguration>
#include <QtNetwork/QSslError>

#include <nx/utils/log/log.h>
#include <nx/utils/random.h>
#include <nx/utils/std_string_utils.h>
#include <nx/utils/string.h>
#include <nx/utils/type_utils.h>

#include "context.h"

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

static void fillHostsFromSubjectAlternativeNames(const X509* x509, std::set<std::string>* hosts)
{
    const auto names = nx::utils::wrapUnique(
        (GENERAL_NAMES*) X509_get_ext_d2i(x509, NID_subject_alt_name, 0, 0),
        &GENERAL_NAMES_free);
    for (int i = 0; i < sk_GENERAL_NAME_num(names.get()); ++i)
    {
        GENERAL_NAME* name = sk_GENERAL_NAME_value(names.get(), i);
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
    }
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

Certificate::Certificate(X509* x509): m_x509(X509_dup(x509), &X509_free)
{
}

bool Certificate::operator==(const Certificate& rhs) const
{
    return X509_cmp(x509(), rhs.x509()) == 0;
}

X509Name Certificate::issuer() const
{
    return X509_get_issuer_name(x509());
}

long Certificate::serialNumber() const
{
    return ASN1_INTEGER_get(X509_get0_serialNumber(x509()));
}

std::chrono::system_clock::time_point Certificate::notBefore() const
{
    return timePoint(X509_get0_notBefore(x509()));
}

std::chrono::system_clock::time_point Certificate::notAfter() const
{
    return timePoint(X509_get0_notAfter(x509()));
}

int Certificate::version() const
{
    return X509_get_version(x509());
}

X509Name Certificate::subject() const
{
    return X509_get_subject_name(x509());
}

std::vector<uint8_t> Certificate::signature() const
{
    const ASN1_BIT_STRING* s = nullptr;
    X509_get0_signature(&s, nullptr, x509());
    return bitStringToByteArray(s);
}

const char* Certificate::signatureAlgorithm() const
{
    return OBJ_nid2ln(X509_get_signature_nid(x509()));
}

std::vector<Certificate::Extension> Certificate::extensions() const
{
    std::vector<Certificate::Extension> result;
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

std::set<std::string> Certificate::hosts() const
{
    std::set<std::string> result;
    fillHostsFromSubjectAlternativeNames(m_x509.get(), &result);
    if (result.empty())
        fillHostsFromLastSubjectCommonName(m_x509.get(), &result);
    return result;
}

std::string Certificate::publicKey() const
{
    EVP_PKEY* pubKey = nullptr;
    if (!NX_ASSERT(pubKey = X509_get0_pubkey(x509())))
        return {};
    const auto bio = nx::utils::wrapUnique(BIO_new(BIO_s_mem()), BIO_free);
    if (!NX_ASSERT(PEM_write_bio_PUBKEY(bio.get(), pubKey) == 1))
        return {};
    return bioToString(bio.get());
}

Certificate::KeyInformation Certificate::publicKeyInformation() const
{
    EVP_PKEY* pkey = nullptr;
    if (!NX_ASSERT(pkey = X509_get0_pubkey(x509())))
        return {};
    return fillPublicKeyInformation(pkey);
}

std::vector<unsigned char> Certificate::sha1() const
{
    const auto digest = EVP_sha1();
    if (NX_ASSERT(digest))
        return fingerprint(x509(), digest);
    return {};
}

std::vector<unsigned char> Certificate::sha256() const
{
    const auto digest = EVP_sha256();
    if (NX_ASSERT(digest))
        return fingerprint(x509(), digest);
    return {};
}

std::string Certificate::printedText() const
{
    auto output = nx::utils::wrapUnique(BIO_new(BIO_s_mem()), &BIO_free);
    X509_print_ex(output.get(), x509(), XN_FLAG_COMPAT, X509_FLAG_COMPAT);
    return bioToString(output.get());
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

bool Certificate::KeyInformation::operator==(const Certificate::KeyInformation& rhs) const
{
    if (algorithmId == rhs.algorithmId
        && modulus == rhs.modulus
        && rsa.has_value() == rhs.rsa.has_value())
    {
        return rsa->bits == rhs.rsa->bits && rsa->exponent == rhs.rsa->exponent;
    }
    return false;
}

bool Certificate::KeyInformation::parsePem(const std::string& str)
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

X509Certificate::X509Certificate(X509* x509):
    m_x509(X509_dup(x509), &X509_free)
{
}

bool X509Certificate::parsePem(
    const std::string& pem,
    std::optional<int> maxChainLength)
{
    auto bio = utils::wrapUnique(
        BIO_new_mem_buf(const_cast<void*>((const void*)pem.data()), pem.size()),
        &BIO_free);

    auto x509 = utils::wrapUnique(PEM_read_bio_X509_AUX(bio.get(), 0, 0, 0), &X509_free);
    auto certSize = i2d_X509(x509.get(), 0);
    if (!x509 || certSize <= 0)
    {
        NX_DEBUG(this, "Unable to read certificate");
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

bool X509Certificate::bindToContext(SSL_CTX* sslContext) const
{
    if (!SSL_CTX_use_certificate(sslContext, m_x509.get()))
        return false;

    for (auto& x509: m_extraChainCerts)
    {
        if (!SSL_CTX_add_extra_chain_cert(sslContext, x509.get()))
        {
            NX_DEBUG(this, "Unable to load chained X.509: %1");
            return false;
        }

        // SSL_CTX_add_extra_chain_cert assumes ownership of the X509 but it does not
        // increment its ref count. That's different from SSL_CTX_use_certificate which
        // invokes X509_up_ref(X509*) itself.
        X509_up_ref(x509.get());
    }

    return true;
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

//-------------------------------------------------------------------------------------------------

Pem::Pem():
    m_pkey(nullptr, &EVP_PKEY_free)
{
}

bool Pem::parse(const std::string& str)
{
    return m_certificate.parsePem(str) && loadPrivateKey(str);
}

bool Pem::bindToContext(SSL_CTX* sslContext) const
{
    if (!m_certificate.bindToContext(sslContext))
    {
        NX_DEBUG(this, "Certificate %1. Unable to bind to SSL context",
            m_certificate.toString());
        return false;
    }

    if (!SSL_CTX_use_PrivateKey(sslContext, m_pkey.get()))
    {
        NX_DEBUG(this, "Certificate %1. Unable to use PKEY", m_certificate.toString());
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

bool Pem::loadPrivateKey(const std::string& pem)
{
    auto bio = utils::wrapUnique(
        BIO_new_mem_buf(const_cast<void*>((const void*)pem.data()), pem.size()),
        &BIO_free);

    m_pkey.reset(PEM_read_bio_PrivateKey(bio.get(), 0, 0, 0));
    if (!m_pkey)
    {
        NX_DEBUG(this, "Unable to read PKEY");
        return false;
    }

    NX_VERBOSE(this, "PKEY is loaded (SSL init is complete)");
    return true;
}

//-------------------------------------------------------------------------------------------------

static const int kRsaLength = 2048;

static bool setIssuerAndSubject(X509* x509, const X509Name& issuerAndSubject)
{
    const auto name = utils::wrapUnique(X509_NAME_new(), &X509_NAME_free);
    if (!NX_ASSERT(name, "Failed to create new X509_NAME"))
        return false;

    const auto setField = [name = name.get()](const std::string& field, const std::string& value)
    {
        return X509_NAME_add_entry_by_txt(
            name, field.c_str(), MBSTRING_UTF8, (unsigned char *) value.data(), -1, -1, 0);
    };

    for (const auto& [name, value]: issuerAndSubject.attrs)
    {
        if (!NX_ASSERT(
            setField(name, value), "Failed to set X509_NAME %1 with value %2", name, value))
        {
            return false;
        }
    }

    if (!NX_ASSERT(X509_set_issuer_name(x509, name.get())))
        return false;

    if (!NX_ASSERT(X509_set_subject_name(x509, name.get())))
        return false;

    return true;
}

static bool setDnsName(X509* x509, const std::string& dnsName)
{
    auto names = utils::wrapUnique(sk_GENERAL_NAME_new_null(), &GENERAL_NAMES_free);
    if (!NX_ASSERT(names, "Failed to create new GENERAL_NAMES"))
        return false;

    auto name = utils::wrapUnique(GENERAL_NAME_new(), &GENERAL_NAME_free);
    if (!NX_ASSERT(name, "Failed to create new GENERAL_NAME"))
        return false;

    auto ia5String = utils::wrapUnique(ASN1_IA5STRING_new(), &ASN1_STRING_free);
    if (!NX_ASSERT(ia5String, "Failed to create new ASN1_IA5STRING"))
        return false;

    if (!NX_ASSERT(ASN1_STRING_set(ia5String.get(), dnsName.data(), dnsName.length())))
        return false;

    GENERAL_NAME_set0_value(name.get(), GEN_DNS, ia5String.get());

    ia5String.release();

    if (!NX_ASSERT(sk_GENERAL_NAME_push(names.get(), name.get())))
        return false;

    name.release();

    if (!NX_ASSERT(X509_add1_ext_i2d(x509, NID_subject_alt_name, names.get(), 0, 0)))
        return false;

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

    auto number = utils::wrapUnique(BN_new(), &BN_free);
    if (!number || !BN_set_word(number.get(), RSA_F4))
    {
        NX_WARNING(typeid(Certificate), "Unable to generate big number");
        return std::string();
    }

    auto rsa = utils::wrapUnique(RSA_new(), &RSA_free);
    if (!rsa || !RSA_generate_key_ex(rsa.get(), kRsaLength, number.get(), NULL))
    {
        NX_WARNING(typeid(Certificate), "Unable to generate RSA");
        return std::string();
    }

    const auto pkey = utils::wrapUnique(EVP_PKEY_new(), &EVP_PKEY_free);
    if (!pkey || !EVP_PKEY_assign_RSA(pkey.get(), rsa.release()))
    {
        NX_WARNING(typeid(Certificate), "Unable to generate PKEY");
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

std::string makeCertificate(
    const PKeyPtr& pkey,
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

    const time_t now = time(nullptr);
    auto x509 = utils::wrapUnique(X509_new(), &X509_free);
    if (!x509
        || !X509_set_version(x509.get(), 2) //< x509.v3
        || !ASN1_INTEGER_set(X509_get_serialNumber(x509.get()), *serialNumber)
        || !ASN1_TIME_adj(
            X509_get_notBefore(x509.get()), now, /*offset_day*/ 0, notBeforeAdjust.count())
        || !ASN1_TIME_adj(
            X509_get_notAfter(x509.get()), now, /*offset_day*/ 0, notAfterAdjust.count())
        || !X509_set_pubkey(x509.get(), pkey.get()))
    {
        NX_WARNING(typeid(Certificate), "Unable to generate X509 cert");
        return std::string();
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
            if (ex && X509_add_ext(x509.get(), ex.get(), -1))
                return true;

            NX_WARNING(typeid(Certificate),
                "Failed to add %1 extension with value %2 to X509 certificate",
                OBJ_nid2ln(nid),
                value);
            return false;
        };

    static const auto kKeyUsage =
        "digitalSignature,nonRepudiation,keyEncipherment,dataEncipherment,keyAgreement,keyCertSign";

    if (!setIssuerAndSubject(x509.get(), issuerAndSubject)
        || (!dnsName.empty() && !setDnsName(x509.get(), dnsName))
        || !addExt(NID_key_usage, kKeyUsage)
        || !addExt(NID_ext_key_usage, "serverAuth"))
    {
        return std::string();
    }

    if (!X509_sign(x509.get(), pkey.get(), EVP_sha256()))
    {
        NX_WARNING(typeid(Certificate), "Failed to sign X509 certificate");
        return std::string();
    }

    std::string result;
    const auto bio = utils::wrapUnique(BIO_new(BIO_s_mem()), BIO_free);
    if (!bio
        || !PEM_write_bio_X509(bio.get(), x509.get())
        || !PEM_write_bio_PrivateKey(bio.get(), pkey.get(), 0, 0, 0, 0, 0)
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
    const std::chrono::seconds& maxDuration)
{
    if (auto pem = readPemFile(filePath); pem && useCertificate(*pem, issuer, maxDuration))
        return true;

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

    return Context::instance()->setDefaultCertificate(certData);
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

void addTrustedRootCertificate(const Certificate& cert)
{
    QSslConfiguration conf = QSslConfiguration::defaultConfiguration();
    conf.addCaCertificate(QSslCertificate(pemByteArray(cert.x509())));
    QSslConfiguration::setDefaultConfiguration(std::move(conf));
}

} // namespace nx::network::ssl

#endif // ENABLE_SSL
