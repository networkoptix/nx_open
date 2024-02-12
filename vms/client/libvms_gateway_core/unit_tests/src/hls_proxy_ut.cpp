// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "test_setup.h"

#include <optional>

#include <nx/network/address_resolver.h>
#include <nx/network/cloud/cloud_stream_socket.h>
#include <nx/network/hls/hls_types.h>
#include <nx/network/http/http_client.h>
#include <nx/network/http/http_content_type.h>
#include <nx/network/http/test_http_server.h>
#include <nx/network/m3u/m3u_playlist.h>
#include <nx/network/socket_global.h>
#include <nx/network/socket_factory.h>
#include <nx/network/ssl/context.h>
#include <nx/network/ssl/ssl_stream_socket.h>

#include <nx/cloud/vms_gateway/vms_gateway_process.h>

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
    virtual std::string getForeignHostName() const override
    {
        return m_foreignHostFullCloudName;
    }

    void setForeignHostFullCloudName(const std::string& hostName)
    {
        m_foreignHostFullCloudName = hostName;
    }

private:
    std::string m_foreignHostFullCloudName;
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
        if (m_socketFactoryBak)
            nx::network::SocketFactory::setCreateStreamSocketFunc(std::move(*m_socketFactoryBak));
    }

protected:
    void setSslEnabled(bool val)
    {
        m_sslEnabled = val;
    }

    void whenReceivedHlsPlaylistViaProxy()
    {
        nx::Buffer msgBody;
        std::string contentType;
        ASSERT_TRUE(nx::network::http::HttpClient::fetchResource(
            playlistProxyUrl(), &msgBody, &contentType, nx::network::kNoTimeout, nx::network::ssl::kAcceptAnyCertificate));
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

        nx::Buffer msgBody;
        std::string contentType;
        ASSERT_TRUE(nx::network::http::HttpClient::fetchResource(
            chunkUrl, &msgBody, &contentType, nx::network::kNoTimeout, nx::network::ssl::kAcceptAnyCertificate));

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
            const auto pathParts = chunkProxyPath.split('/', Qt::SkipEmptyParts);
            const auto targetHostName = pathParts[1];
            ASSERT_EQ(m_hlsServerFullCloudName, targetHostName);
        }
    }

private:
    nx::network::http::TestHttpServer m_hlsServer;
    nx::network::m3u::Playlist m_receivedPlaylist;
    std::string m_hlsServerFullCloudName;
    std::string m_cloudSystemId;
    std::string m_cloudServerId;
    std::optional<nx::network::SocketFactory::CreateStreamSocketFuncType> m_socketFactoryBak;
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

        m_cloudSystemId = nx::Uuid::createUuid().toSimpleStdString();
        m_cloudServerId = nx::Uuid::createUuid().toSimpleStdString();
        m_hlsServerFullCloudName = nx::format("%1.%2").arg(m_cloudServerId).arg(m_cloudSystemId).toStdString();
        nx::network::SocketGlobals::addressResolver().addFixedAddress(
            m_hlsServerFullCloudName,
            m_hlsServer.serverAddress());
        nx::network::SocketGlobals::addressResolver().addFixedAddress(
            m_cloudSystemId,
            m_hlsServer.serverAddress());

        m_socketFactoryBak = nx::network::SocketFactory::setCreateStreamSocketFunc(
            std::bind(&HlsProxy::createStreamSocket, this, _2));

        m_hlsServer.registerStaticProcessor(
            kHlsChunkPath,
            kHlsChunkContents,
            kHlsChunkContentType);

        network::hls::Chunk hlsChunk;
        hlsChunk.duration = 10;
        hlsChunk.url = nx::format("%1").arg(kHlsChunkPath);

        network::hls::Playlist playlist;
        playlist.closed = true;
        playlist.chunks.push_back(hlsChunk);

        m_hlsServer.registerStaticProcessor(
            kHlsPlaylistPath,
            playlist.toString(),
            nx::network::http::kAudioMpegUrlMimeType);
    }

    nx::utils::Url playlistProxyUrl() const
    {
        return nx::utils::Url(nx::format("http://%1/%2%3")
            .arg(moduleInstance()->impl()->httpEndpoints()[0])
            .arg(m_cloudSystemId).arg(kHlsPlaylistPath));
    }

    std::unique_ptr<nx::network::AbstractStreamSocket> createStreamSocket(bool sslRequired)
    {
        auto socket = std::make_unique<TestCloudStreamSocket>();
        socket->setForeignHostFullCloudName(m_hlsServerFullCloudName);

        if (m_sslEnabled && sslRequired)
        {
            return nx::network::ssl::kAcceptAnyCertificate(std::move(socket));
        }
        else
        {
            return socket;
        }
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
