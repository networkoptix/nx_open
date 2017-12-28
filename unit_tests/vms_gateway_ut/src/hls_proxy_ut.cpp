#include "test_setup.h"

#include <nx/network/address_resolver.h>
#include <nx/network/cloud/cloud_stream_socket.h>
#include <nx/network/hls/hls_types.h>
#include <nx/network/http/http_client.h>
#include <nx/network/http/http_content_type.h>
#include <nx/network/http/test_http_server.h>
#include <nx/network/m3u/m3u_playlist.h>
#include <nx/network/socket_global.h>
#include <nx/network/socket_factory.h>
#include <nx/network/ssl_socket.h>

#include <vms_gateway_process.h>

namespace nx {
namespace cloud {
namespace gateway {
namespace test {

namespace {

const char* const kHlsPlaylistPath = "/playlist.m3u";
const char* const kHlsChunkPath = "/hls/chunk.ts";

const char* const kHlsChunkContents = "This could be mpeg2/ts file.";
const char* const kHlsChunkContentType = "video/mp2t";

class TestCloudStreamSocket:
    public nx::network::cloud::CloudStreamSocket
{
    using base_type = nx::network::cloud::CloudStreamSocket;

public:
    virtual QString getForeignHostName() const override
    {
        return m_foreignHostFullCloudName;
    }

    void setForeignHostFullCloudName(const QString& hostName)
    {
        m_foreignHostFullCloudName = hostName;
    }

private:
    QString m_foreignHostFullCloudName;
};

} // namespace

//-------------------------------------------------------------------------------------------------

class HlsProxy:
    public BasicComponentTest
{
public:
    HlsProxy()
    {
        addArg("-http/allowTargetEndpointInUrl", "true");
    }

    ~HlsProxy()
    {
        SocketFactory::setCreateStreamSocketFunc(std::move(m_socketFactoryBak));
    }

protected:
    void setSslEnabled(bool val)
    {
        m_sslEnabled = val;
    }

    void whenReceivedHlsPlaylistViaProxy()
    {
        nx::network::http::BufferType msgBody;
        nx::network::http::StringType contentType;
        ASSERT_TRUE(nx::network::http::HttpClient::fetchResource(
            playlistProxyUrl(), &msgBody, &contentType));
        ASSERT_TRUE(m_receivedPlaylist.parse(msgBody));
    }

    void thenPlaylistItemIsAvailableThroughProxy()
    {
        nx::utils::Url chunkUrl;
        for (const auto& entry: m_receivedPlaylist.entries)
        {
            if (entry.type == network::m3u::EntryType::location)
                chunkUrl = nx::utils::Url(entry.value);
        }

        if (chunkUrl.host().isEmpty())
        {
            chunkUrl.setHost(moduleInstance()->impl()->httpEndpoints()[0].address.toString());
            chunkUrl.setPort(moduleInstance()->impl()->httpEndpoints()[0].port);
            chunkUrl.setScheme("http");
        }

        nx::network::http::BufferType msgBody;
        nx::network::http::StringType contentType;
        ASSERT_TRUE(nx::network::http::HttpClient::fetchResource(
            chunkUrl, &msgBody, &contentType));

        ASSERT_EQ(kHlsChunkContents, msgBody);
        ASSERT_EQ(kHlsChunkContentType, contentType);
    }

    void thenAllPlaylistItemsPointToTheExactServer()
    {
        for (const auto& entry: m_receivedPlaylist.entries)
        {
            if (entry.type != network::m3u::EntryType::location)
                continue;
            QString chunkProxyPath = nx::utils::Url(entry.value).path();
            const auto pathParts = chunkProxyPath.split('/', QString::SkipEmptyParts);
            const auto targetHostName = pathParts[1];
            ASSERT_EQ(m_hlsServerFullCloudName, targetHostName);
        }
    }

private:
    TestHttpServer m_hlsServer;
    nx::network::m3u::Playlist m_receivedPlaylist;
    QString m_hlsServerFullCloudName;
    QString m_cloudSystemId;
    QString m_cloudServerId;
    SocketFactory::CreateStreamSocketFuncType m_socketFactoryBak;
    bool m_sslEnabled = false;

    virtual void SetUp() override
    {
        ASSERT_TRUE(startAndWaitUntilStarted());

        launchHlsServer();
    }

    void launchHlsServer()
    {
        using namespace std::placeholders;

        ASSERT_TRUE(m_hlsServer.bindAndListen());

        m_cloudSystemId = QnUuid::createUuid().toSimpleString();
        m_cloudServerId = QnUuid::createUuid().toSimpleString();
        m_hlsServerFullCloudName = lm("%1.%2").arg(m_cloudServerId).arg(m_cloudSystemId);
        nx::network::SocketGlobals::addressResolver().addFixedAddress(
            m_hlsServerFullCloudName,
            m_hlsServer.serverAddress());
        nx::network::SocketGlobals::addressResolver().addFixedAddress(
            m_cloudSystemId,
            m_hlsServer.serverAddress());

        m_socketFactoryBak = SocketFactory::setCreateStreamSocketFunc(
            std::bind(&HlsProxy::createStreamSocket, this, _1, _2));

        m_hlsServer.registerStaticProcessor(
            kHlsChunkPath,
            kHlsChunkContents,
            kHlsChunkContentType);

        nx_hls::Chunk hlsChunk;
        hlsChunk.duration = 10;
        hlsChunk.url = lm("%1").arg(kHlsChunkPath);

        nx_hls::Playlist playlist;
        playlist.closed = true;
        playlist.chunks.push_back(hlsChunk);

        m_hlsServer.registerStaticProcessor(
            kHlsPlaylistPath,
            playlist.toString(),
            nx::network::http::kAudioMpegUrlMimeType);
    }

    nx::utils::Url playlistProxyUrl() const
    {
        return nx::utils::Url(lm("http://%1/%2%3")
            .arg(moduleInstance()->impl()->httpEndpoints()[0])
            .arg(m_cloudSystemId).arg(kHlsPlaylistPath));
    }

    std::unique_ptr<AbstractStreamSocket> createStreamSocket(
        bool sslRequired,
        nx::network::NatTraversalSupport /*natTraversalRequired*/)
    {
        auto socket = std::make_unique<TestCloudStreamSocket>();
        socket->setForeignHostFullCloudName(m_hlsServerFullCloudName);

        if (m_sslEnabled && sslRequired)
            return std::make_unique<nx::network::deprecated::SslSocket>(std::move(socket), false);
        else
            return std::move(socket);
    }
};

//-------------------------------------------------------------------------------------------------

TEST_F(HlsProxy, playlist_urls_are_modified)
{
    whenReceivedHlsPlaylistViaProxy();
    thenPlaylistItemIsAvailableThroughProxy();
}

TEST_F(HlsProxy, subsequent_requests_are_forwarded_to_the_same_server)
{
    whenReceivedHlsPlaylistViaProxy();
    thenAllPlaylistItemsPointToTheExactServer();
}

TEST_F(HlsProxy, subsequent_requests_are_forwarded_to_the_same_server_when_ssl_is_used)
{
    setSslEnabled(true);

    whenReceivedHlsPlaylistViaProxy();
    thenAllPlaylistItemsPointToTheExactServer();
}

} // namespace test
} // namespace gateway
} // namespace cloud
} // namespace nx
