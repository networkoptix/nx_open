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
     * Implements https://docs.aws.amazon.com/general/latest/gr/sigv4_signing.html#Query string.
     *
     * The request is expected to have the following headers with non-empty values:
     * x-amz-content-sha256
     * x-amz-date
     *
     * @return Value of the Authorization Query string.
     */
     std::tuple<nx::String, bool /*result*/> calculateAuthorizationHeader(
        const network::http::Request& request,
        const network::http::Credentials& credentials,
        const std::string& region,
        const std::string& service);

    /**
     * @return Hex(HMAC-SHA256(SigningKey, StringToSign)).
     */
     std::tuple<nx::String, bool /*result*/> calculateSignature(
        const network::http::Request& request,
        const network::http::Credentials& credentials,
        const std::string& region,
        const std::string& service,
        IntermediateValues* intermediateValues = nullptr);

protected:
    virtual void hashPath(nx::utils::QnCryptographicHash* hash, const QString& path);

private:
     nx::Buffer calculateSignKey(
        const nx::String& date,
        const network::http::Credentials& credentials,
        const std::string& region,
        const std::string& service);

     std::tuple<nx::Buffer, bool /*result*/> calculateCanonicalRequestSha256Hash(
        const network::http::Request& request,
        IntermediateValues* intermediateValues);

     void hashQuery(nx::utils::QnCryptographicHash* hash, const nx::String& query);

     void hashCanonicalHeaders(
        nx::utils::QnCryptographicHash* hash,
        const nx::network::http::HttpHeaders& headers,
        std::vector<nx::String>* lowCaseHeaderNames);

     void hashSignedHeaders(
        nx::utils::QnCryptographicHash* hash,
        const std::vector<nx::String>& lowCaseHeaderNames,
        IntermediateValues* intermediateValues);

     bool addScope(
        HMAC_CTX* hmacCtx,
        const nx::String& date,
        const std::string& region,
        const std::string& service);
};

} // namespace nx::cloud::aws
