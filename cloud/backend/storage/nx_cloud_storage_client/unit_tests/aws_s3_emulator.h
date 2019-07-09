#pragma once

#include <map>
#include <string>

#include <nx/network/http/test_http_server.h>
#include <nx/utils/thread/mutex.h>

namespace nx::cloud::storage::client::aws_s3::test {

class AwsS3Emulator
{
public:
    AwsS3Emulator();

    bool bindAndListen(const nx::network::SocketAddress& endpoint);
    nx::network::SocketAddress serverAddress() const;

    nx::utils::Url baseApiUrl() const;

    void saveOrReplaceFile(
        const std::string& path,
        nx::Buffer contents);

    /**
     * @return std::nullopt if no file exists under path.
     */
    std::optional<nx::Buffer> getFile(const std::string& path) const;

private:
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

} // namespace nx::cloud::storage::client::aws_s3::test
