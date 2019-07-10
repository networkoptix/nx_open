#pragma once

#include <string>

#include <openssl/hmac.h>

#include <nx/network/buffer.h>
#include <nx/network/http/auth_tools.h>

namespace nx::cloud::storage::client::aws_s3 {

class NX_CLOUD_STORAGE_CLIENT_API SignatureCalculator
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
    static nx::String calculateAuthorizationHeader(
        const network::http::Request& request,
        const network::http::Credentials& credentials,
        const std::string& region,
        const std::string& service);

    /**
     * @return Hex(HMAC-SHA256(SigningKey, StringToSign)).
     */
    static nx::String calculateSignature(
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

    static nx::Buffer calculateCanonicalRequestSha256Hash(
        const network::http::Request& request,
        IntermediateValues* intermediateValues);

    static bool addScope(
        HMAC_CTX* hmacCtx,
        const nx::String& date,
        const std::string& region,
        const std::string& service);
};

} // namespace nx::cloud::storage::client::aws_s3
