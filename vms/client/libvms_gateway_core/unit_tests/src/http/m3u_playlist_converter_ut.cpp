// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include <gtest/gtest.h>

#include <nx/network/hls/hls_types.h>
#include <nx/network/http/server/proxy/m3u_playlist_converter.h>
#include <nx/utils/std/cpp14.h>

#include <nx/cloud/vms_gateway/http/url_rewriter.h>

namespace nx {
namespace cloud {
namespace gateway {
namespace test {

namespace {

static const char* const kProxyHostUrl = "http://proxy.nxvms.com";

static const char* const kServerHostName = "server_host_name";

static const char* const kOriginalPlaylist =
    "#EXTM3U\r\n"
    "#EXTINF:123,BANDWIDTH=10000\r\n"
    "http://server.host/hls/camera.m3u?hi\r\n"
    "#EXTINF:321,BANDWIDTH=50000\r\n"
    "/hls/camera.m3u?lo\r\n";

static const char* const kExpectedModifiedPlaylist =
    "#EXTM3U\r\n"
    "#EXTINF:123,BANDWIDTH=10000\r\n"
    "http://proxy.nxvms.com/gateway/server_host_name/hls/camera.m3u?hi\r\n"
    "#EXTINF:321,BANDWIDTH=50000\r\n"
    "/gateway/server_host_name/hls/camera.m3u?lo\r\n";

} // namespace

class M3uPlaylistConverter:
    public ::testing::Test
{
public:
    M3uPlaylistConverter()
    {
        m_converter = std::make_unique<nx::network::http::server::proxy::M3uPlaylistConverter>(
            m_urlRewriter,
            kProxyHostUrl,
            kServerHostName);
    }

protected:
    void whenConvertedPlaylist(const std::string& kOriginalPlaylist)
    {
        m_resultingPlaylist = m_converter->convert(kOriginalPlaylist);
    }

    void thenResultingPlaylistEqualTo(const std::string& kExpectedModifiedPlaylist)
    {
        ASSERT_EQ(kExpectedModifiedPlaylist, m_resultingPlaylist);
    }

private:
    UrlRewriter m_urlRewriter;
    std::unique_ptr<nx::network::http::server::proxy::M3uPlaylistConverter> m_converter;
    nx::Buffer m_resultingPlaylist;
};

TEST_F(M3uPlaylistConverter, target_host_address_is_inserted)
{
    whenConvertedPlaylist(kOriginalPlaylist);
    thenResultingPlaylistEqualTo(kExpectedModifiedPlaylist);
}

} // namespace test
} // namespace gateway
} // namespace cloud
} // namespace nx
