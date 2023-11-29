// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "certificate_mac.h"

#include <Security/SecCertificate.h>
#include <Security/SecPolicy.h>
#include <Security/SecTrust.h>

#include <openssl/x509v3.h>

#include <QtCore/QString>

#include <nx/utils/type_utils.h>
#include <nx/utils/std_string_utils.h>

#if defined(ENABLE_SSL)

namespace nx::network::ssl::detail {

namespace {

static SecCertificateRef x509ToSecCertificate(X509* x509)
{
    const auto bio = nx::utils::wrapUnique(BIO_new(BIO_s_mem()), &BIO_free);
    if (i2d_X509_bio(bio.get(), x509) == 0)
        return nullptr;

    BUF_MEM* mem = nullptr;
    const auto result = BIO_get_mem_ptr(bio.get(), &mem);
    NX_ASSERT(result == 1 && mem && mem->length > 0,
        "BIO_get_mem_ptr(bio, &mem): result %1, mem: %2, mem->length: %3",
        result, mem, mem ? mem->length : 0);

    CFDataRef data = CFDataCreate(nullptr, (const UInt8*) mem->data, mem->length);
    SecCertificateRef certificate = SecCertificateCreateWithData(nullptr, data);
    CFRelease(data);

    return certificate;
}

} // namespace

bool verifyBySystemCertificatesMac(
    STACK_OF(X509)* chain, const std::string& hostName, std::string* outErrorMessage)
{
    if (outErrorMessage)
        outErrorMessage->clear();

    std::string trimmedHost = hostName;
    while (trimmedHost.ends_with('.'))
        trimmedHost.pop_back();

    std::vector<SecCertificateRef> certificates;

    const int chainSize = sk_X509_num(chain);
    for (int i = 0; i < chainSize; ++i)
        certificates.push_back(x509ToSecCertificate(sk_X509_value(chain, i)));

    CFArrayRef certificateArray =
        CFArrayCreate(nullptr, (const void**) certificates.data(), certificates.size(), nullptr);

    SecPolicyRef policy = SecPolicyCreateSSL(
        true, CFStringCreateWithCString(nullptr, hostName.c_str(), kCFStringEncodingUTF8));

    SecTrustRef trust;
    CFErrorRef error;

    const bool success =
        SecTrustCreateWithCertificates(certificateArray, policy, &trust) == errSecSuccess
            && SecTrustEvaluateWithError(trust, &error) == true;

    CFRelease(policy);
    CFRelease(trust);

    for (SecCertificateRef& certificate: certificates)
    {
        if (certificate)
            CFRelease(certificate);
    }

    if (!success && outErrorMessage)
    {
        CFStringRef errorDescription = CFErrorCopyDescription(error);
        const auto description = QString::fromCFString(errorDescription);
        CFRelease(errorDescription);

        *outErrorMessage = nx::utils::buildString(
            "Verify certificate for host '",
            hostName,
            "' failed. Error: ",
            description.toStdString());
    }

    if (error)
        CFRelease(error);

    return success;
}

} // namespace nx::network::ssl::detail

#endif // defined(ENABLE_SSL)
