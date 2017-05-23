#include <gtest/gtest.h>

#include <nx/network/hls/hls_types.h>

#include <http/m3u_playlist_converter.h>

namespace nx {
namespace cloud {
namespace gateway {
namespace test {

namespace {

const char* proxyHost = "proxy.nxvms.com";

const char* serverHostName = "server_host_name";

const char* originalPlaylist =
    "#EXTM3U\r\n"
    "#EXTINF:123,BANDWIDTH=10000\r\n"
    "http://server.host/hls/camera.m3u?hi\r\n"
    "#EXTINF:321,BANDWIDTH=50000\r\n"
    "/hls/camera.m3u?lo\r\n";

const char* expectedModifiedPlaylist =
    "#EXTM3U\r\n"
    "#EXTINF:123,BANDWIDTH=10000\r\n"
    "http://proxy.nxvms.com/server_host_name/hls/camera.m3u?hi\r\n"
    "#EXTINF:321,BANDWIDTH=50000\r\n"
    "/server_host_name/hls/camera.m3u?lo\r\n";

} // namespace

class M3uPlaylistConverter:
    public ::testing::Test
{
public:
    M3uPlaylistConverter()
    {
        m_converter = std::make_unique<gateway::M3uPlaylistConverter>(
            proxyHost,
            serverHostName);
    }

protected:
    void whenConvertedPlaylist(const nx::String& originalPlaylist)
    {
        m_resultingPlaylist = m_converter->convert(originalPlaylist);
    }

    void thenResultingPlaylistEqualTo(const nx::String& expectedModifiedPlaylist)
    {
        ASSERT_EQ(expectedModifiedPlaylist, m_resultingPlaylist);
    }

private:
    std::unique_ptr<gateway::M3uPlaylistConverter> m_converter;
    nx_http::BufferType m_resultingPlaylist;
};

TEST_F(M3uPlaylistConverter, target_host_address_is_inserted)
{
    whenConvertedPlaylist(originalPlaylist);
    thenResultingPlaylistEqualTo(expectedModifiedPlaylist);
}

} // namespace test
} // namespace gateway
} // namespace cloud
} // namespace nx
