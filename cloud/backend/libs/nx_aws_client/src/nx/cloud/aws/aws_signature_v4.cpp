#include "aws_signature_v4.h"

#include <algorithm>
#include <cctype>
#include <set>
#include <string_view>

#include <openssl/sha.h>

#include <nx/network/url/url_parse_helper.h>
#include <nx/utils/cryptographic_hash.h>

namespace nx::cloud::aws {

static const std::set<std::string_view, std::greater<std::string_view>>
    kForbiddenCanonicalHeaderPrefixes =
{
    "authorization",
    "connection",
    "forwarded",
    "x-forwarded-",
    "via",
};

static bool isAllowedCanonicalHeader(const std::string_view& name)
{
    auto it = kForbiddenCanonicalHeaderPrefixes.lower_bound(name);
    if (it == kForbiddenCanonicalHeaderPrefixes.end())
        return true;

    return name.substr(0, it->size()) != *it; //< !name.starts_with(*it)
}

//-------------------------------------------------------------------------------------------------

static const nx::String kAwsAuthName = "AWS4-HMAC-SHA256";
static const nx::String kAws4Request = "aws4_request";

std::tuple<nx::String, bool /*result*/> SignatureCalculator::calculateAuthorizationHeader(
    const network::http::Request& request,
    const Credentials& credentials,
    const std::string& region,
    const std::string& service)
{
    IntermediateValues intermediateValues;
    const auto [signature, result] = calculateSignature(
        request, credentials, region, service, &intermediateValues);
    if (!result)
        return std::make_tuple(nx::String(), false);

    const auto authorizaton = nx::String("AWS4-HMAC-SHA256 ") +
        "Credential=" + credentials.username.toUtf8() + "/" + intermediateValues.scope + ","
        "SignedHeaders=" + intermediateValues.signedHeaders + ","
        "Signature=" + signature;

    return std::make_tuple(std::move(authorizaton), true);
}

std::tuple<nx::String, bool /*result*/> SignatureCalculator::calculateSignature(
    const network::http::Request& request,
    const Credentials& credentials,
    const std::string& region,
    const std::string& service,
    IntermediateValues* intermediateValues)
{
    const auto xAmzDateIter = request.headers.find("x-amz-date");
    if (xAmzDateIter == request.headers.end())
        return std::make_tuple(nx::String(), false);

    const auto& timestamp = xAmzDateIter->second;
    const auto tokens = timestamp.split('T');
    if (tokens.isEmpty())
        return std::make_tuple(nx::String(), false);
    const auto& date = tokens[0];

    const auto signKey = calculateSignKey(date, credentials, region, service);

    HMAC_CTX hmacCtx;
    memset(&hmacCtx, 0, sizeof(hmacCtx));
    if (!HMAC_Init_ex(&hmacCtx, signKey.data(), signKey.size(), EVP_sha256(), NULL))
        return std::make_tuple(nx::String(), false);

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

    const auto [requestHash, result] =
        calculateCanonicalRequestSha256Hash(request, intermediateValues);
    if (!result)
        return std::make_tuple(nx::String(), false);
    HMAC_Update(&hmacCtx, (const unsigned char*) requestHash.data(), requestHash.size());

    nx::String signature(SHA256_DIGEST_LENGTH, 0);
    unsigned int signatureLength = signature.size();
    HMAC_Final(&hmacCtx, (unsigned char*) signature.data(), &signatureLength);

    return std::make_tuple(signature.toHex(), true);
}

nx::Buffer SignatureCalculator::calculateSignKey(
    const nx::String& date,
    const Credentials& credentials,
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

std::tuple<nx::Buffer, bool /*result*/>
    SignatureCalculator::calculateCanonicalRequestSha256Hash(
        const network::http::Request& request,
        IntermediateValues* intermediateValues)
{
    nx::utils::QnCryptographicHash cryptographicHash(
        nx::utils::QnCryptographicHash::Algorithm::Sha256);

    cryptographicHash.addData(request.requestLine.method);
    cryptographicHash.addData("\n");

    hashPath(&cryptographicHash, request.requestLine.url.path());
    cryptographicHash.addData("\n");

    hashQuery(&cryptographicHash, request.requestLine.url.query().toUtf8());
    cryptographicHash.addData("\n");

    // Canonical headers.
    std::vector<nx::String> lowCaseHeaderNames;
    hashCanonicalHeaders(&cryptographicHash, request.headers, &lowCaseHeaderNames);
    cryptographicHash.addData("\n");

    // Signed headers.
    hashSignedHeaders(&cryptographicHash, lowCaseHeaderNames, intermediateValues);
    cryptographicHash.addData("\n");

    // Payload hash.
    auto it = request.headers.find("x-amz-content-sha256");
    if (it != request.headers.end())
    {
        cryptographicHash.addData(it->second);
    }
    else
    {
        // Calculating payload signature.
        const auto payloadHash = nx::utils::QnCryptographicHash::hash(
            request.messageBody,
            nx::utils::QnCryptographicHash::Algorithm::Sha256).toHex().toLower();
        cryptographicHash.addData(payloadHash);
    }

    return std::make_tuple(cryptographicHash.result().toHex(), true);
}

QByteArray SignatureCalculator::encodePath(const QString& path)
{
    return QUrl::toPercentEncoding(QUrl::toPercentEncoding(path, "/"), "/");
}

template<typename CryptographicHash>
void SignatureCalculator::hashPath(
    CryptographicHash* hash, const QString& path)
{
    const auto encodedPath = encodePath(nx::network::url::normalizePath(path));

    if (!encodedPath.startsWith('/'))
        hash->addData("/");
    hash->addData(encodedPath);
}

template<typename CryptographicHash>
void SignatureCalculator::hashQuery(
    CryptographicHash* hash,
    const nx::String& query)
{
    if (query.isEmpty())
        return;

    auto queryItems = query.split('&');
    std::sort(queryItems.begin(), queryItems.end());
    for (int i = 0; i < queryItems.size(); ++i)
    {
        if (i > 0)
            hash->addData("&");

        const auto tokens = queryItems[i].split('=');
        if (!tokens.isEmpty())
        {
            hash->addData(QUrl::toPercentEncoding(tokens[0]));
            hash->addData("=");
        }
        if (tokens.size() > 1)
            hash->addData(QUrl::toPercentEncoding(tokens[1]));
    }
}

template<typename CryptographicHash>
void SignatureCalculator::hashCanonicalHeaders(
    CryptographicHash* hash,
    const nx::network::http::HttpHeaders& headers,
    std::vector<nx::String>* lowCaseHeaderNames)
{
    // TODO: #ak Headers that occur multiple times should be put as:
    // my-header1:value4,value1,value3,value2

    lowCaseHeaderNames->reserve(headers.size());
    for (const auto& header: headers)
    {
        const auto lowerName = header.first.toLower();
        if (!isAllowedCanonicalHeader(std::string_view(lowerName.data(), lowerName.size())))
            continue;

        auto trimmedHeaderValue = trimHeaderValue(header.second);

        if (!lowCaseHeaderNames->empty() && lowCaseHeaderNames->back() == lowerName)
        {
            hash->addData(",");
            hash->addData(trimmedHeaderValue);
        }
        else
        {
            if (!lowCaseHeaderNames->empty())
                hash->addData("\n"); //< Closing the previous header.

            lowCaseHeaderNames->push_back(lowerName);

            hash->addData(lowCaseHeaderNames->back());
            hash->addData(":");
            hash->addData(trimmedHeaderValue);
        }
    }

    if (!lowCaseHeaderNames->empty())
        hash->addData("\n"); //< Closing the previous header.
}

template<typename CryptographicHash>
void SignatureCalculator::hashSignedHeaders(
    CryptographicHash* hash,
    const std::vector<nx::String>& lowCaseHeaderNames,
    IntermediateValues* intermediateValues)
{
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
    hash->addData(signedHeaders);

    if (intermediateValues)
        intermediateValues->signedHeaders = std::move(signedHeaders);
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

QByteArray SignatureCalculator::trimHeaderValue(const QByteArray& str)
{
    QByteArray result;
    result.reserve(str.size());

    bool prevChIsSpace = true; //< setting to true to remove spaces in the beginning.
    for (int i = 0; i < str.size(); ++i)
    {
        const bool chIsSpace = std::isspace(str[i]);
        if (!(prevChIsSpace && chIsSpace))
            result.push_back(str[i]);
        // else: skipping subsequent spaces.

        prevChIsSpace = chIsSpace;
    }

    if (result.endsWith(' '))
        result.remove(result.size() - 1, 1);

    return result;
}

} // namespace nx::cloud::aws
