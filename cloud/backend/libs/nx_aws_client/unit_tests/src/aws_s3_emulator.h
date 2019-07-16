#pragma once

#include <map>
#include <string>

#include <nx/network/http/server/abstract_authentication_manager.h>
#include <nx/network/http/test_http_server.h>
#include <nx/utils/thread/mutex.h>

namespace nx::cloud::aws::test {

/**
 * Requires a request to be signed using AWS Signature V4
 * (https://docs.aws.amazon.com/AmazonS3/latest/API/sig-v4-header-based-auth.html).
 */
class AwsSignatureV4Authenticator:
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

class AwsS3Emulator
{
public:
    AwsS3Emulator();

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

private:
    AwsSignatureV4Authenticator m_awsAuthenticator;
    nx::network::http::TestHttpServer m_httpServer;
    std::map<std::string, nx::Buffer> m_pathToFileContents;
    mutable QnMutex m_mutex;

    void registerHttpApi();

    void saveFile(
        nx::network::http::RequestContext requestContext,
        nx::network::http::RequestProcessedHandler completionHandler);

    void getFile(
        nx::network::http::RequestContext requestContext,
        nx::network::http::RequestProcessedHandler completionHandler);
};

} // namespace nx::cloud::aws::test
