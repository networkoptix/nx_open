#include "aws_signature_v4.h"

#include <openssl/sha.h>

#include <nx/utils/cryptographic_hash.h>

namespace nx::cloud::storage::client::aws_s3 {

static const nx::String kAwsAuthName = "AWS4-HMAC-SHA256";
static const nx::String kAws4Request = "aws4_request";

nx::String SignatureCalculator::calculateAuthorizationHeader(
    const network::http::Request& request,
    const network::http::Credentials& credentials,
    const std::string& region,
    const std::string& service)
{
    IntermediateValues intermediateValues;
    const auto signature = calculateSignature(
        request, credentials, region, service, &intermediateValues);

    return nx::String("AWS4-HMAC-SHA256 ") +
        "Credential=" + credentials.username.toUtf8() + "/" + intermediateValues.scope + ","
        "SignedHeaders=" + intermediateValues.signedHeaders + ","
        "Signature=" + signature;
}

nx::String SignatureCalculator::calculateSignature(
    const network::http::Request& request,
    const network::http::Credentials& credentials,
    const std::string& region,
    const std::string& service,
    IntermediateValues* intermediateValues)
{
    const auto xAmzDateIter = request.headers.find("x-amz-date");
    if (xAmzDateIter == request.headers.end())
        return nx::String();

    const auto& timestamp = xAmzDateIter->second;
    const auto tokens = timestamp.split('T');
    if (tokens.isEmpty())
        return nx::String();
    const auto& date = tokens[0];

    const auto signKey = calculateSignKey(date, credentials, region, service);

    HMAC_CTX hmacCtx;
    memset(&hmacCtx, 0, sizeof(hmacCtx));
    if (!HMAC_Init_ex(&hmacCtx, signKey.data(), signKey.size(), EVP_sha256(), NULL))
        return nx::String();

    HMAC_Update(&hmacCtx, (const unsigned char*) kAwsAuthName.data(), kAwsAuthName.size());
    HMAC_Update(&hmacCtx, (const unsigned char*) "\n", 1);
    HMAC_Update(&hmacCtx, (const unsigned char*) timestamp.data(), timestamp.size());
    HMAC_Update(&hmacCtx, (const unsigned char*) "\n", 1);
    const auto scope =
        date + "/" + region.c_str() + "/" + service.c_str() + "/" + kAws4Request;
    HMAC_Update(&hmacCtx, (const unsigned char*) scope.data(), scope.size());
    HMAC_Update(&hmacCtx, (const unsigned char*) "\n", 1);
    if (intermediateValues)
        intermediateValues->scope = scope;

    const auto requestHash = calculateCanonicalRequestSha256Hash(request, intermediateValues);
    HMAC_Update(&hmacCtx, (const unsigned char*) requestHash.data(), requestHash.size());

    nx::String signature(SHA256_DIGEST_LENGTH, 0);
    unsigned int signatureLength = signature.size();
    HMAC_Final(&hmacCtx, (unsigned char*) signature.data(), &signatureLength);

    return signature.toHex();
}

nx::Buffer SignatureCalculator::calculateSignKey(
    const nx::String& date,
    const network::http::Credentials& credentials,
    const std::string& region,
    const std::string& service)
{
    nx::Buffer md(SHA256_DIGEST_LENGTH, 0);
    unsigned int mdLen = md.size();

    const auto key = "AWS4" + credentials.authToken.value;
    HMAC(EVP_sha256(),
        key.data(), key.size(),
        (const unsigned char*) date.data(), date.size(),
        (unsigned char*) md.data(), &mdLen);
    // md stores DateKey

    HMAC(EVP_sha256(),
        md.data(), md.size(),
        (const unsigned char*) region.data(), region.size(),
        (unsigned char*) md.data(), &mdLen);
    // md stores DateRegionKey

    HMAC(EVP_sha256(),
        md.data(), md.size(),
        (const unsigned char*) service.data(), service.size(),
        (unsigned char*) md.data(), &mdLen);
    // md stores DateRegionServiceKey

    HMAC(EVP_sha256(),
        md.data(), md.size(),
        (const unsigned char*) kAws4Request.data(), kAws4Request.size(),
        (unsigned char*) md.data(), &mdLen);
    // md stores SigningKey

    return md;
}

nx::Buffer SignatureCalculator::calculateCanonicalRequestSha256Hash(
    const network::http::Request& request,
    IntermediateValues* intermediateValues)
{
    nx::utils::QnCryptographicHash cryptographicHash(
        nx::utils::QnCryptographicHash::Algorithm::Sha256);

    cryptographicHash.addData(request.requestLine.method);
    cryptographicHash.addData("\n");

    // TODO: #ak UriEncode.
    cryptographicHash.addData(request.requestLine.url.path().toUtf8());
    cryptographicHash.addData("\n");

    cryptographicHash.addData(request.requestLine.url.query().toUtf8());
    cryptographicHash.addData("\n");

    // Canonical headers.
    std::vector<nx::String> lowCaseHeaderNames;
    lowCaseHeaderNames.reserve(request.headers.size());
    for (const auto& header: request.headers)
    {
        lowCaseHeaderNames.push_back(header.first.toLower());

        cryptographicHash.addData(lowCaseHeaderNames.back());
        cryptographicHash.addData(":");
        cryptographicHash.addData(header.second.trimmed());
        cryptographicHash.addData("\n");
    }
    cryptographicHash.addData("\n");

    // Signed headers.
    nx::String signedHeaders;
    bool first = true;
    for (const auto& name: lowCaseHeaderNames)
    {
        if (first)
            first = false;
        else
            signedHeaders += ";";

        signedHeaders += name;
    }
    cryptographicHash.addData(signedHeaders);
    cryptographicHash.addData("\n");

    if (intermediateValues)
        intermediateValues->signedHeaders = std::move(signedHeaders);

    // Payload hash.
    auto it = request.headers.find("x-amz-content-sha256");
    if (it == request.headers.end())
        return nx::Buffer();
    cryptographicHash.addData(it->second);

    return cryptographicHash.result().toHex();
}

bool SignatureCalculator::addScope(
    HMAC_CTX* hmacCtx,
    const nx::String& date,
    const std::string& region,
    const std::string& service)
{
    HMAC_Update(hmacCtx, (const unsigned char*) date.data(), date.size());
    HMAC_Update(hmacCtx, (const unsigned char*) "/", 1);
    HMAC_Update(hmacCtx, (const unsigned char*) region.data(), region.size());
    HMAC_Update(hmacCtx, (const unsigned char*) "/", 1);
    HMAC_Update(hmacCtx, (const unsigned char*) service.data(), service.size());
    HMAC_Update(hmacCtx, (const unsigned char*) "/", 1);
    HMAC_Update(hmacCtx, (const unsigned char*) kAws4Request.data(), kAws4Request.size());

    return true;
}

} // namespace nx::cloud::storage::client::aws_s3
