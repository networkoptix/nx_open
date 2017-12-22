#include <gtest/gtest.h>
#include <nx/mediaserver/rest/updates2_rest_handler.h>
#include <nx/update/info/detail/data_provider/raw_data_provider_factory.h>
#include <nx/update/info/detail/data_provider/test_support/impl/async_json_provider_mockup.h>
#include <test_support/mediaserver_launcher.h>

namespace nx {
namespace mediaserver {
namespace rest {
namespace test {

using namespace update::info::detail::data_provider;

class Updates2RestHandler: public ::testing::Test
{
protected:
    virtual void SetUp() override
    {
        setUpMediaServer();
        mockupUpdateProvider();
    }

private:
    std::unique_ptr<MediaServerLauncher> m_mediaServerLauncher = nullptr;

    void setUpMediaServer()
    {
        m_mediaServerLauncher.reset(new MediaServerLauncher());
        ASSERT_TRUE(m_mediaServerLauncher->start());
    }

    static void mockupUpdateProvider()
    {
        RawDataProviderFactory::setFactoryFunction(
            [](const QString& s, AbstractAsyncRawDataProviderHandler* handler)
            {
                return std::make_unique<test_support::AsyncJsonProviderMockup>(handler);
            });
    }
};

} // namespace test
} // namespace rest
} // namespace mediaserver
} // namespace nx