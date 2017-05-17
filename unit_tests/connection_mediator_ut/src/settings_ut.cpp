#include <gtest/gtest.h>

#include <settings.h>

namespace nx {
namespace hpm {
namespace conf {

class SettingsCloudDbUrl:
    public ::testing::Test
{
public:
    SettingsCloudDbUrl():
        m_args{"--cloud_db/runWithCloud=true"}
    {
    }

protected:
    void whenPassedCloudDbUrl(const std::string& value)
    {
        m_args.push_back("--cloud_db/url=" + value);
    }

    void whenPassedStringAsCloudDbEndpoint(const std::string& settingValue)
    {
        m_args.push_back("--cloud_db/endpoint=" + settingValue);
    }

    void assertCloudDbUrlEqualTo(const boost::optional<QUrl>& expected)
    {
        loadSettings();

        ASSERT_EQ(expected, m_settings.cloudDB().url);
    }

private:
    std::vector<std::string> m_args;
    Settings m_settings;

    void loadSettings()
    {
        std::vector<const char*> args;
        args.reserve(m_args.size());
        for (const auto& arg: m_args)
            args.push_back(arg.c_str());

        m_settings.load((int)args.size(), &args[0]);
    }
};

TEST_F(SettingsCloudDbUrl, no_url)
{
    whenPassedStringAsCloudDbEndpoint("");
    assertCloudDbUrlEqualTo(boost::none);
}

TEST_F(SettingsCloudDbUrl, parsing_url_passed_to_endpoint_setting)
{
    whenPassedStringAsCloudDbEndpoint("http://cloud-test.hdw.mx:33461");
    assertCloudDbUrlEqualTo(QUrl("http://cloud-test.hdw.mx:33461"));
}

TEST_F(SettingsCloudDbUrl, parsing_endpoint_setting)
{
    whenPassedStringAsCloudDbEndpoint("cloud-test.hdw.mx:33461");
    assertCloudDbUrlEqualTo(QUrl("http://cloud-test.hdw.mx:33461"));
}

TEST_F(SettingsCloudDbUrl, endpoint_setting_is_ignored_ifurl_is_present)
{
    whenPassedCloudDbUrl("http://cloud-dev.hdw.mx:12345");
    whenPassedStringAsCloudDbEndpoint("cloud-test.hdw.mx:33461");
    assertCloudDbUrlEqualTo(QUrl("http://cloud-dev.hdw.mx:12345"));
}

TEST_F(SettingsCloudDbUrl, passing_only_url)
{
    whenPassedCloudDbUrl("http://cloud-dev.hdw.mx:12345");
    assertCloudDbUrlEqualTo(QUrl("http://cloud-dev.hdw.mx:12345"));
}

} // namespace conf
} // namespace hpm
} // namespace nx
