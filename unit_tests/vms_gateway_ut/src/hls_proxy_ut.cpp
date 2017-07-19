#include "test_setup.h"

#include <nx/network/hls/hls_types.h>
#include <nx/network/http/http_client.h>
#include <nx/network/http/http_content_type.h>
#include <nx/network/http/test_http_server.h>
#include <nx/network/m3u/m3u_playlist.h>

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

} // namespace

class HlsProxy:
    public BasicComponentTest
{
public:
    HlsProxy()
    {
        addArg("-http/allowTargetEndpointInUrl", "true");
    }

protected:
    void whenReceivedHlsPlaylistViaProxy()
    {
        nx_http::BufferType msgBody;
        nx_http::StringType contentType;
        ASSERT_TRUE(nx_http::HttpClient::fetchResource(
            playlistProxyUrl(), &msgBody, &contentType));
        ASSERT_TRUE(m_receivedPlaylist.parse(msgBody));
    }

    void thenPlaylistItemIsAvailableThroughProxy()
    {
        QUrl chunkUrl;
        for (const auto& entry: m_receivedPlaylist.entries)
        {
            if (entry.type == network::m3u::EntryType::location)
                chunkUrl = QUrl(entry.value);
        }

        nx_http::BufferType msgBody;
        nx_http::StringType contentType;
        ASSERT_TRUE(nx_http::HttpClient::fetchResource(
            chunkUrl, &msgBody, &contentType));

        ASSERT_EQ(kHlsChunkContents, msgBody);
        ASSERT_EQ(kHlsChunkContentType, contentType);
    }

private:
    TestHttpServer m_hlsServer;
    nx::network::m3u::Playlist m_receivedPlaylist;

    virtual void SetUp() override
    {
        ASSERT_TRUE(startAndWaitUntilStarted());

        launchHlsServer();
    }

    void launchHlsServer()
    {
        ASSERT_TRUE(m_hlsServer.bindAndListen());

        m_hlsServer.registerStaticProcessor(
            kHlsChunkPath,
            kHlsChunkContents,
            kHlsChunkContentType);

        nx_hls::Chunk hlsChunk;
        hlsChunk.duration = 10;
        hlsChunk.url = lm("http://%1%2").arg(m_hlsServer.serverAddress()).arg(kHlsChunkPath);
         
        nx_hls::Playlist playlist;
        playlist.closed = true;
        playlist.chunks.push_back(hlsChunk);

        m_hlsServer.registerStaticProcessor(
            kHlsPlaylistPath,
            playlist.toString(),
            nx_http::kAudioMpegUrlMimeType);
    }

    QUrl playlistProxyUrl() const
    {
        return QUrl(lm("http://%1/%2%3")
            .arg(moduleInstance()->impl()->httpEndpoints()[0])
            .arg(m_hlsServer.serverAddress()).arg(kHlsPlaylistPath));
    }
};

TEST_F(HlsProxy, playlist_urls_are_modified)
{
    whenReceivedHlsPlaylistViaProxy();
    thenPlaylistItemIsAvailableThroughProxy();
}

} // namespace test
} // namespace gateway
} // namespace cloud
} // namespace nx
