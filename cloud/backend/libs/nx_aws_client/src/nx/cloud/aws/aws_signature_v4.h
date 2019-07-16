#pragma once

#include <string>
#include <tuple>

#include <openssl/hmac.h>

#include <nx/network/buffer.h>
#include <nx/network/http/auth_tools.h>

namespace nx::utils { class QnCryptographicHash; }

namespace nx::cloud::aws {

class NX_AWS_CLIENT_API SignatureCalculator
{
public:
    struct IntermediateValues
    {
        nx::String scope;
        nx::String signedHeaders;
    };

    /**
     * Implements https://docs.aws.amazon.com/AmazonS3/latest/API/sig-v4-header-based-auth.html.
     *
     * The request is expected to have the following headers with non-empty values:
     * x-amz-content-sha256
     * x-amz-date
     *
     * @return Value of the Authorization header.
     */
    static std::tuple<nx::String, bool /*result*/> calculateAuthorizationHeader(
        const network::http::Request& request,
        const network::http::Credentials& credentials,
        const std::string& region,
        const std::string& service);

    /**
     * @return Hex(HMAC-SHA256(SigningKey, StringToSign)).
     */
    static std::tuple<nx::String, bool /*result*/> calculateSignature(
        const network::http::Request& request,
        const network::http::Credentials& credentials,
        const std::string& region,
        const std::string& service,
        IntermediateValues* intermediateValues = nullptr);

private:

    static nx::Buffer calculateSignKey(
        const nx::String& date,
        const network::http::Credentials& credentials,
        const std::string& region,
        const std::string& service);

    static std::tuple<nx::Buffer, bool /*result*/> calculateCanonicalRequestSha256Hash(
        const network::http::Request& request,
        IntermediateValues* intermediateValues);

    static void hashPath(nx::utils::QnCryptographicHash* hash, const QString& path);
    static void hashQuery(nx::utils::QnCryptographicHash* hash, const nx::String& query);

    static void hashCanonicalHeaders(
        nx::utils::QnCryptographicHash* hash,
        const nx::network::http::HttpHeaders& headers,
        std::vector<nx::String>* lowCaseHeaderNames);

    static void hashSignedHeaders(
        nx::utils::QnCryptographicHash* hash,
        const std::vector<nx::String>& lowCaseHeaderNames,
        IntermediateValues* intermediateValues);

    static bool addScope(
        HMAC_CTX* hmacCtx,
        const nx::String& date,
        const std::string& region,
        const std::string& service);
};

} // namespace nx::cloud::aws
