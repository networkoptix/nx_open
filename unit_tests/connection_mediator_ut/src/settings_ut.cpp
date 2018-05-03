#include <initializer_list>
#include <vector>

#include <gtest/gtest.h>

#include <settings.h>

namespace nx {
namespace hpm {
namespace conf {
namespace test {

class Settings:
    public ::testing::Test
{
public:
    Settings() = default;

    Settings(std::initializer_list<std::string> args):
        m_args(std::move(args))
    {
    }

    void addSetting(const std::string& setting)
    {
        m_args.push_back(setting);
    }

protected:
    void loadSettings()
    {
        std::vector<const char*> args;
        args.reserve(m_args.size());
        for (const auto& arg : m_args)
            args.push_back(arg.c_str());

        m_settings.load((int)args.size(), &args[0]);
    }

    const conf::Settings& settings() const
    {
        return m_settings;
    }

private:
    std::vector<std::string> m_args;
    conf::Settings m_settings;
};

//-------------------------------------------------------------------------------------------------

class SettingsCloudDbUrl:
    public Settings
{
public:
    SettingsCloudDbUrl():
        Settings({"--cloud_db/runWithCloud=true"})
    {
    }

protected:
    void whenPassedCloudDbUrl(const std::string& value)
    {
        addSetting("--cloud_db/url=" + value);
    }

    void whenPassedStringAsCloudDbEndpoint(const std::string& settingValue)
    {
        addSetting("--cloud_db/endpoint=" + settingValue);
    }

    void assertCloudDbUrlEqualTo(const boost::optional<nx::utils::Url>& expected)
    {
        loadSettings();

        ASSERT_EQ(expected, settings().cloudDB().url);
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
    assertCloudDbUrlEqualTo(nx::utils::Url("http://cloud-test.hdw.mx:33461"));
}

TEST_F(SettingsCloudDbUrl, parsing_endpoint_setting)
{
    whenPassedStringAsCloudDbEndpoint("cloud-test.hdw.mx:33461");
    assertCloudDbUrlEqualTo(nx::utils::Url("http://cloud-test.hdw.mx:33461"));
}

TEST_F(SettingsCloudDbUrl, endpoint_setting_is_ignored_ifurl_is_present)
{
    whenPassedCloudDbUrl("http://cloud-dev.hdw.mx:12345");
    whenPassedStringAsCloudDbEndpoint("cloud-test.hdw.mx:33461");
    assertCloudDbUrlEqualTo(nx::utils::Url("http://cloud-dev.hdw.mx:12345"));
}

TEST_F(SettingsCloudDbUrl, passing_only_url)
{
    whenPassedCloudDbUrl("http://cloud-dev.hdw.mx:12345");
    assertCloudDbUrlEqualTo(nx::utils::Url("http://cloud-dev.hdw.mx:12345"));
}

//-------------------------------------------------------------------------------------------------

// TODO: nat traversal method delay.
// TODO: ConnectResponse serialization/deserialization

TEST_F(Settings, nat_traversal_method_delay)
{
    using namespace std::chrono;

    addSetting("--cloudConnect/udpHolePunchingStartDelay=47ms");
    addSetting("--cloudConnect/trafficRelayingStartDelay=13s");
    addSetting("--cloudConnect/directTcpConnectStartDelay=999ms");

    loadSettings();

    ASSERT_EQ(milliseconds(47), settings().connectionParameters().udpHolePunchingStartDelay);
    ASSERT_EQ(seconds(13), settings().connectionParameters().trafficRelayingStartDelay);
    ASSERT_EQ(milliseconds(999), settings().connectionParameters().directTcpConnectStartDelay);
}

TEST_F(Settings, traffic_relay_url)
{
    const std::string relayUrl = "http://nxvms.com/relay";

    addSetting("--trafficRelay/url=" + relayUrl);
    loadSettings();
    ASSERT_EQ(relayUrl, settings().trafficRelay().url.toStdString());
}

} // namespace test
} // namespace conf
} // namespace hpm
} // namespace nx
