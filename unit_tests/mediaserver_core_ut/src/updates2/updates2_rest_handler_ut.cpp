#include <gtest/gtest.h>
#include <nx/mediaserver/rest/updates2_rest_handler.h>
#include <test_support/mediaserver_launcher.h>

namespace nx {
namespace mediaserver {
namespace rest {
namespace test {

class Updates2RestHandler: public ::testing::Test
{
protected:
    virtual void SetUp() override
    {
        setUpMediaServer();
    }

private:
    std::unique_ptr<MediaServerLauncher> m_mediaServerLauncher;

    void setUpMediaServer()
    {
        m_mediaServerLauncher.reset(new MediaServerLauncher());
        ASSERT_TRUE(m_mediaServerLauncher->start());
    }
};

} // namespace test
} // namespace rest
} // namespace mediaserver
} // namespace nx