#pragma once

#include <map>
#include <string>

#include <nx/network/http/server/abstract_authentication_manager.h>
#include <nx/network/http/test_http_server.h>
#include <nx/utils/thread/mutex.h>

#include <nx/cloud/aws/s3/list_bucket_request.h>

namespace nx::cloud::aws::s3::test {

/**
 * Requires a request to be signed using AWS Signature V4
 * (https://docs.aws.amazon.com/AmazonS3/latest/API/sig-v4-header-based-auth.html).
 */
class NX_AWS_CLIENT_API AwsSignatureV4Authenticator:
    public nx::network::http::server::AbstractAuthenticationManager
{
public:
    virtual void authenticate(
        const nx::network::http::HttpServerConnection& connection,
        const nx::network::http::Request& request,
        nx::network::http::server::AuthenticationCompletionHandler completionHandler) override;

    void addCredentials(
        const std::string& accessKeyId,
        const std::string& secretAccessKey);

private:
    std::map<std::string /*accessKeyId*/, std::string /*secretAccessKey*/> m_credentials;

    nx::network::http::server::AuthenticationResult prepareUnsuccesfulResult();
};

//-------------------------------------------------------------------------------------------------

static constexpr char kDefaultS3Location[] = "us-east-1";

static const std::vector<std::string> kS3Locations = {
    "us-east-2", "us-east-1", "us-west-2", "us-west-1", "ap-east-1", "ap-south-1",
    "ap-northeast-3", "ap-northeast-2", "ap-southeast-1", "ap-southeast-2", "ap-northeast-1",
    "ca-central-1", "cn-north-1", "cn-northwest-1", "eu-central-1", "eu-west-1", "eu-west-2",
    "eu-west-3", "eu-north-1", "sa-east-1"
};

NX_AWS_CLIENT_API std::string randomS3Location();

/**
 * Emulates some AWS S3 API calls.
 * NOTE: Bucket object operations emulate a single bucket.
 */
class NX_AWS_CLIENT_API AwsS3Emulator
{
public:
    AwsS3Emulator(std::string location = kDefaultS3Location);

    bool bindAndListen(const nx::network::SocketAddress& endpoint);
    nx::network::SocketAddress serverAddress() const;

    void enableAthentication(
        const std::regex& path,
        nx::network::http::Credentials credentials);

    nx::utils::Url baseApiUrl() const;

    void saveOrReplaceFile(
        const std::string& path,
        nx::Buffer contents);

    /**
     * @return std::nullopt if no file exists under path.
     */
    std::optional<nx::Buffer> getFile(const std::string& path) const;

    /**
     * @return true if file was deleted. false if file now found.
     */
    bool deleteFile(const std::string& path);

    /**
     * @return the region of the bucket, e.g. "us-east-1"
     */
    std::string location() const;
    void setLocation(const std::string& location);

private:
    AwsSignatureV4Authenticator m_awsAuthenticator;
    nx::network::http::TestHttpServer m_httpServer;
    std::map<std::string, nx::Buffer> m_pathToFileContents;
    std::string m_location;
    mutable QnMutex m_mutex;

    void registerHttpApi();

    void saveFile(
        nx::network::http::RequestContext requestContext,
        nx::network::http::RequestProcessedHandler completionHandler);

    void getFile(
        nx::network::http::RequestContext requestContext,
        nx::network::http::RequestProcessedHandler completionHandler);

    void deleteFile(
        nx::network::http::RequestContext requestContext,
        nx::network::http::RequestProcessedHandler completionHandler);

    void getLocation(
        nx::network::http::RequestContext requestContext,
        nx::network::http::RequestProcessedHandler completionHandler);

    void listBucket(
        nx::network::http::RequestContext requestContext,
        nx::network::http::RequestProcessedHandler completionHandler);

    void dispatchRootPathGetRequest(
        nx::network::http::RequestContext requestContext,
        nx::network::http::RequestProcessedHandler completionHandler);

    ListBucketResult getListBucketResult(std::map<QString, QString> queries) const;

    std::map<QString, QString> parseQuery(const QString& query) const;
};

} // namespace nx::cloud::aws::s3::test
