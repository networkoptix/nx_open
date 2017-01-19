#include <gtest/gtest.h>

#include <settings.h>

namespace nx {
namespace hpm {
namespace conf {

class SettingsCloudDbUrl:
    public ::testing::Test
{
protected:
    void whenPassedStringAsCloudDbEndpoint(const std::string& settingValue)
    {
        std::vector<const char*> args{"--cloud_db/runWithCloud=true"};

        std::string endpointSetting; 
        if (!settingValue.empty())
        {
            endpointSetting = "--cloud_db/endpoint=" + settingValue;
            args.push_back(endpointSetting.c_str());
        }

        m_settings.load((int)args.size(), &args[0]);
    }

    void assertIfCloudDbUrlNotEqualTo(const boost::optional<QUrl>& expected)
    {
        ASSERT_EQ(expected, m_settings.cloudDB().url);
    }

private:
    Settings m_settings;
};

TEST_F(SettingsCloudDbUrl, no_url)
{
    whenPassedStringAsCloudDbEndpoint("");
    assertIfCloudDbUrlNotEqualTo(boost::none);
}

TEST_F(SettingsCloudDbUrl, parsing_url_string)
{
    whenPassedStringAsCloudDbEndpoint("http://cloud-test.hdw.mx:33461");
    assertIfCloudDbUrlNotEqualTo(QUrl("http://cloud-test.hdw.mx:33461"));
}

TEST_F(SettingsCloudDbUrl, parsing_endpoint_string)
{
    whenPassedStringAsCloudDbEndpoint("cloud-test.hdw.mx:33461");
    assertIfCloudDbUrlNotEqualTo(QUrl("http://cloud-test.hdw.mx:33461"));
}

} // namespace conf
} // namespace hpm
} // namespace nx
