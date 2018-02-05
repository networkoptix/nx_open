#include <algorithm>

#include <gtest/gtest.h>

#include <nx/network/m3u/m3u_playlist.h>

namespace nx {
namespace network {
namespace m3u {
namespace test {

class M3uPlayList:
    public ::testing::Test
{
protected:
    void parse(const char* str)
    {
        ASSERT_TRUE(m_playlist.parse(str));
    }

    void assertDirectiveIsPresent(const nx::String& directive)
    {
        assertEntryIsPresent(EntryType::directive, directive);
    }

    void assertLocationIsPresent(const nx::String& location)
    {
        assertEntryIsPresent(EntryType::location, location);
    }

    void assertParsedRepresentationsAreEqual(
        const char* left,
        const char* right)
    {
        m3u::Playlist leftPlaylist;
        ASSERT_TRUE(leftPlaylist.parse(left));

        m3u::Playlist rightPlaylist;
        ASSERT_TRUE(rightPlaylist.parse(right));

        ASSERT_EQ(leftPlaylist, rightPlaylist);
    }

    void assertPlaylistIsEmpty()
    {
        ASSERT_TRUE(m_playlist.entries.empty());
    }

    void assertStringRepresentationIsEqualTo(const nx::String& str)
    {
        ASSERT_EQ(str, m_playlist.toString());
    }

private:
    m3u::Playlist m_playlist;

    void assertEntryIsPresent(EntryType type, const nx::String& value)
    {
        auto it = std::find_if(
            m_playlist.entries.begin(), m_playlist.entries.end(),
            [&type, &value](const Entry& entry)
            {
                return entry.type == type && entry.value == value;
            });
        ASSERT_NE(m_playlist.entries.end(), it);
    }
};

namespace {

const char* usualData =
    "#EXTM3U\r\n"
    "\r\n"
    "#EXTINF:123, Sample artist - Sample title\r\n"
    "C:\\Documents and Settings\\I\\My Music\\Sample.mp3\r\n"
    "\r\n"
    "#EXTINF:321,Example Artist - Example title\r\n"
    "C:\\Documents and Settings\\I\\My Music\\Greatest Hits\\Example.ogg\r\n";

const char* dataWithEmptyLines =
    "\r\n"
    "#EXTM3U\r\n"
    "\r\n"
    "C:\\Documents and Settings\\I\\My Music\\Sample.mp3\r\n"
    "\r\n";

const char* sameDataWithoutEmptyLines =
    "#EXTM3U\r\n"
    "C:\\Documents and Settings\\I\\My Music\\Sample.mp3\r\n";

const char* usualDataWithoutEmptyLines =
    "#EXTM3U\r\n"
    "#EXTINF:123, Sample artist - Sample title\r\n"
    "C:\\Documents and Settings\\I\\My Music\\Sample.mp3\r\n"
    "#EXTINF:321,Example Artist - Example title\r\n"
    "C:\\Documents and Settings\\I\\My Music\\Greatest Hits\\Example.ogg\r\n";

} // namespace

TEST_F(M3uPlayList, parse)
{
    parse(usualData);

    assertDirectiveIsPresent("#EXTM3U");
    assertDirectiveIsPresent("#EXTINF:123, Sample artist - Sample title");
    assertLocationIsPresent("C:\\Documents and Settings\\I\\My Music\\Sample.mp3");

    assertDirectiveIsPresent("#EXTINF:321,Example Artist - Example title");
    assertLocationIsPresent("C:\\Documents and Settings\\I\\My Music\\Greatest Hits\\Example.ogg");
}

TEST_F(M3uPlayList, parse_skips_empty_lines)
{
    assertParsedRepresentationsAreEqual(
        dataWithEmptyLines,
        sameDataWithoutEmptyLines);
}

TEST_F(M3uPlayList, parse_empty_string)
{
    parse("");
    assertPlaylistIsEmpty();
}

TEST_F(M3uPlayList, toString)
{
    parse(usualDataWithoutEmptyLines);
    assertStringRepresentationIsEqualTo(usualDataWithoutEmptyLines);
}

} // namespace test
} // namespace m3u
} // namespace network
} // namespace nx
