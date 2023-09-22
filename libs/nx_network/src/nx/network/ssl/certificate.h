// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#ifdef ENABLE_SSL

#include <chrono>
#include <map>
#include <optional>
#include <set>
#include <string>
#include <vector>

#include <openssl/ssl.h>

#include <nx/network/socket_common.h>
#include <nx/utils/buffer.h>
#include <nx/utils/string.h>

namespace nx::network::ssl {

// https://cabforum.org/2017/03/17/ballot-193-825-day-certificate-lifetimes/
// https://support.apple.com/en-us/HT211025
constexpr auto kCertMaxDuration = std::chrono::hours(24) * 397;

constexpr int kRsaLength = 2048;

struct X509Name
{
    /**
     * map<name, value>.
     * Typical fields:
     * CN - common name
     * C - country code
     * O - organization
     * OU - organization unit
     */
    std::map<std::string, std::string> attrs;

    X509Name() = default;
    X509Name(const struct X509_name_st* name);

    X509Name(
        const std::string& commonName,
        const std::string& country,
        const std::string& organization,
        const std::string& organizationUnit = {})
    {
        attrs.emplace(SN_commonName, commonName);
        attrs.emplace(SN_countryName, country);
        attrs.emplace(SN_organizationName, organization);
        if (!organizationUnit.empty())
            attrs.emplace(SN_organizationalUnitName, organizationUnit);
    }

    std::string toString() const
    {
        return nx::utils::join(
            attrs.begin(), attrs.end(), ",",
            [](const auto& attr) { return nx::utils::buildString(attr.first, "=", attr.second); });
    }

    bool operator==(const X509Name& right) const
    {
        return attrs == right.attrs;
    }
};

inline std::ostream& operator<<(std::ostream& os, const X509Name& issuer)
{
    os << issuer.toString();
    return os;
}

//-------------------------------------------------------------------------------------------------

/**
 * Non-owning certificate wrapper.
 */
class NX_NETWORK_API CertificateView
{
public:
    struct Extension
    {
        int nameId = NID_undef;
        std::string value;
        bool isCritical = false;

        const char* name() const { return OBJ_nid2ln(nameId); }

        bool operator==(const Extension& rhs) const
        {
            return nameId == rhs.nameId && value == rhs.value && isCritical == rhs.isCritical;
        }
    };

    struct NX_NETWORK_API KeyInformation
    {
        struct Rsa
        {
            std::vector<uint8_t> exponent;
            int bits = 0;
        };

        int algorithmId = NID_undef;
        std::vector<uint8_t> modulus;
        std::optional<Rsa> rsa;

        bool operator==(const KeyInformation& rhs) const;

        bool operator!=(const KeyInformation& rhs) const
        {
            return !(*this == rhs);
        }

        const char* algorithm() const { return OBJ_nid2ln(algorithmId); }

        bool parsePem(const std::string& str);
    };

    CertificateView(X509* x509);

    X509Name issuer() const;
    long serialNumber() const;
    std::chrono::system_clock::time_point notBefore() const;
    std::chrono::system_clock::time_point notAfter() const;
    int version() const; //< Correct version is in the range [0:2].
    X509Name subject() const;
    std::vector<uint8_t> signature() const;
    const char* signatureAlgorithm() const;
    std::string publicKey() const;
    KeyInformation publicKeyInformation() const;
    std::vector<Extension> extensions() const;
    std::set<std::string> hosts() const;

    /**
     * Generates user-readable text representation of the "Subject Alternative Names" extension of
     * the certificate (same as openssl x509 -text).
     */
    std::string subjectAltNames() const;

    std::vector<unsigned char> sha1() const;
    std::vector<unsigned char> sha256() const;

    /**
     * Generates user-readable text representation of the certificate (same as openssl x509 -text).
     */
    std::string printedText() const;

    X509* x509() const { return m_x509; }

private:
    X509* m_x509;
};

NX_NETWORK_API bool operator==(const CertificateView& lhs, const CertificateView& rhs);

/**
 * Owning certificate wrapper.
 */
class NX_NETWORK_API Certificate: public CertificateView
{
public:
    using Extension = CertificateView::Extension;
    using KeyInformation = CertificateView::KeyInformation;

    Certificate(X509* x509 = nullptr);
    explicit Certificate(const CertificateView& view);

    static std::vector<Certificate> parse(
        const std::string& pemString,
        bool assertOnFail = true);

private:
    std::shared_ptr<X509> m_x509;
};

inline std::ostream& operator<<(std::ostream& os, const Certificate& cert)
{
    os << "Serial Number:\n" <<
        "    " << cert.serialNumber() << "\n"
        "Issuer: " << cert.issuer() << "\n";
    return os;
}

//-------------------------------------------------------------------------------------------------

/**
 * Wraps X.509 openssl certificate object.
 */
class NX_NETWORK_API X509Certificate
{
public:
    X509Certificate(X509* x509 = nullptr);
    X509Certificate(const X509Certificate& certificate);
    X509Certificate& operator=(const X509Certificate& certificate);

    /**
     * @return true if certificate was parsed successfully.
     * WARNING: This function reads only certificate from the pem, not the private key.
     */
    bool parsePem(
        const std::string& buf,
        std::optional<int> maxChainLength = std::nullopt,
        std::string* errorMessage = nullptr);

    /**
     * Validates the certificate against the current local time.
     */
    bool isValid(const std::chrono::seconds& maxDuration = kCertMaxDuration) const;

    /**
     * @return (notAfter - notBefore). std::nullopt if certificate is invalid or does not have
     * both dates.
     */
    std::optional<std::chrono::seconds> duration() const;

    bool isSignedBy(const X509Name& issuer) const;

    bool bindToContext(SSL_CTX* sslContext, std::string* errorMessage = nullptr) const;

    std::string toString() const;
    std::string idForToStringFromPtr() const;
    std::string pemString() const;
    std::vector<Certificate> certificates() const;
    std::set<std::string> hosts() const;
    X509* x509() const;

    /**
     * Generates user-readable text representation of the "Subject Alternative Names" extension of
     * the certificate (same as openssl x509 -text).
     */
    std::string subjectAltNames() const;

private:
    static std::string toString(const X509* x509);
    static std::string toString(const ASN1_TIME* time);

private:
    using X509Ptr = std::unique_ptr<X509, decltype(&X509_free)>;

    X509Ptr m_x509;
    std::vector<X509Ptr> m_extraChainCerts;
};

//-------------------------------------------------------------------------------------------------

using PKeyPtr = std::unique_ptr<EVP_PKEY, decltype(&EVP_PKEY_free)>;

/**
 * Wraps PEM (X.509 certificate + private key)
 */
class NX_NETWORK_API Pem
{
public:
    Pem();
    Pem(const Pem& pem);
    Pem& operator=(const Pem& pem);

    bool parse(const std::string& str, std::string* errorMessage = nullptr);

    bool bindToContext(SSL_CTX* sslContext, std::string* errorMessage = nullptr) const;

    std::string toString() const;

    X509Certificate& certificate();
    const X509Certificate& certificate() const;

    const PKeyPtr& privateKey() const { return m_pkey; }

private:
    bool loadPrivateKey(const std::string& pem, std::string* errorMessage = nullptr);

private:
    X509Certificate m_certificate;
    PKeyPtr m_pkey;
};

//-------------------------------------------------------------------------------------------------

static constexpr char kLocalhostCertificateAlternativeNames[] =
    "localhost, IP Address:127.0.0.1, IP Address:::1";

/**
 * @param serialNumber If not specified then a random number is used.
 * @param hostName Specifies the list of hostnames and/or IP addresses, which will be included in
 *     the certificate as Subject Alternative Names. The string has the following format:
 *     name[, name, ..., name]
 *     where name must have one of the following formats:
 *         - <host> or DNS:<host> - DNS-name,
 *         - IP Address:<ip> - IPv4 or IPv6 address.
 * @return Certificate and private key in PEM format.
 */
NX_NETWORK_API std::string makeCertificateAndKey(
    const X509Name& issuerAndSubject,
    const std::string& hostName = kLocalhostCertificateAlternativeNames,
    std::optional<long> serialNumber = std::nullopt,
    std::chrono::seconds notBeforeAdjust = std::chrono::seconds::zero(),
    std::chrono::seconds notAfterAdjust = kCertMaxDuration);

NX_NETWORK_API bool makeCertificateAndKeyFile(
    const std::string& filePath,
    const X509Name& issuerAndSubject,
    const std::string& hostName = kLocalhostCertificateAlternativeNames,
    std::optional<long> serialNumber = std::nullopt,
    std::chrono::seconds notBeforeAdjust = std::chrono::seconds::zero(),
    std::chrono::seconds notAfterAdjust = kCertMaxDuration);

NX_NETWORK_API PKeyPtr generateKey(int length = kRsaLength);

NX_NETWORK_API bool generateKeyFiles(
    const std::string& privateKeyFileName,
    const std::string& publicKeyFileName,
    int rsaKeyLength = kRsaLength);

NX_NETWORK_API std::string makeCertificate(
    const PKeyPtr& privateKey,
    const X509Name& issuerAndSubject,
    const std::string& hostName,
    std::optional<long> serialNumber = std::nullopt,
    std::chrono::seconds notBeforeAdjust = std::chrono::seconds::zero(),
    std::chrono::seconds notAfterAdjust = kCertMaxDuration);

NX_NETWORK_API std::string makeCertificate(
    const PKeyPtr& keyPair,
    const PKeyPtr& signingKey,
    const X509Name& subject,
    std::optional<CertificateView> issuerCertificate,
    const std::string& hostName,
    std::optional<long> serialNumber = std::nullopt,
    std::chrono::seconds notBeforeAdjust = std::chrono::seconds::zero(),
    std::chrono::seconds notAfterAdjust = kCertMaxDuration,
    std::optional<bool> isCa = std::nullopt);

/**
 * If the certificate is valid and is generated by the issuer, then sets the certificate as default
 * and returns true. Otherwise returns false.
 */
NX_NETWORK_API bool useCertificate(
    const Pem& pem,
    const X509Name& issuer,
    const std::chrono::seconds& maxDuration = kCertMaxDuration);

/**
 * Loads certificate from filePath and sets it as default with
 * Context::instance()->setDefaultCertificate call.
 * If file is NOT found OR the certificate IS NOT valid OR generated by issuer AND expired,
 * then a new certificate is generated and written to filePath.
 * Optional argument certificate can be used for reading out the certificate that has been set.
 * @return true a valid certificate was read from filePath or a certificate was generated
 * and set as default one.
 */
NX_NETWORK_API bool useOrCreateCertificate(
    const std::string& filePath,
    const X509Name& issuer,
    const std::string& hostName = kLocalhostCertificateAlternativeNames,
    const std::chrono::seconds& maxDuration = kCertMaxDuration,
    std::string* certificate = nullptr);

/**
 * Loads certificate from filePath and sets it as default.
 * @return true if a certificate was read from the file and was successfully set as the default one.
 */
NX_NETWORK_API bool loadCertificateFromFile(const std::string& filePath);

/**
 * Generates certificate and sets it as default.
 */
NX_NETWORK_API void useRandomCertificate(
    const std::string& module_,
    const std::string& hostName = kLocalhostCertificateAlternativeNames);

NX_NETWORK_API std::optional<Pem> readPemFile(const std::string& filePath);

NX_NETWORK_API bool verifyBySystemCertificates(
    STACK_OF(X509)* chain, const std::string& hostName, std::string* outErrorMessage = nullptr);

NX_NETWORK_API std::vector<Certificate> completeCertificateChain(
    STACK_OF(X509)* chain, bool* ok = nullptr);

NX_NETWORK_API void addTrustedRootCertificate(const CertificateView& cert);

} // namespace nx::network::ssl

#endif // ENABLE_SSL
