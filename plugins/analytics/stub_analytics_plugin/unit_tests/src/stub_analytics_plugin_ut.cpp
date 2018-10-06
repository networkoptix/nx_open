#include <iostream>
#include <vector>

#include <nx/kit/debug.h>
#include <nx/kit/test.h>

#include <plugins/plugin_api.h>
#include <plugins/plugin_tools.h>

#include <nx/sdk/analytics/engine.h>
#include <nx/sdk/analytics/consuming_device_agent.h>
#include <nx/sdk/analytics/uncompressed_video_frame.h>
#include <nx/sdk/analytics/compressed_video_packet.h>
#include <nx/sdk/analytics/engine.h>
#include <nx/sdk/analytics/plugin.h>

extern "C" {

nxpl::PluginInterface* createNxAnalyticsPlugin();

} // extern "C"

static std::string trimString(const std::string& s)
{
    int start = 0;
    while (start < (int) s.size() && s.at(start) <= ' ')
        ++start;
    int end = s.size() - 1;
    while (end >= 0 && s.at(end) <= ' ')
        --end;
    if (end < start)
        return "";
    return s.substr(start, end - start + 1);
}

static const int noError = (int) nx::sdk::Error::noError;

static void testEngineManifest(nx::sdk::analytics::Engine* engine)
{
    nx::sdk::Error error = nx::sdk::Error::noError;
    const char* manifest = engine->manifest(&error);
    ASSERT_TRUE(manifest != nullptr);
    ASSERT_EQ((int) nx::sdk::Error::noError, (int) error);
    ASSERT_TRUE(manifest[0] != '\0');
    NX_PRINT << "Engine manifest:\n" << manifest;

    const std::string trimmedEngineManifest = trimString(manifest);
    ASSERT_EQ('{', trimmedEngineManifest.front());
    ASSERT_EQ('}', trimmedEngineManifest.back());

    // This test assumes that the plugin consumes compressed frames - verify it in the manifest.
    ASSERT_EQ(std::string::npos, std::string(manifest).find("needUncompressedVideoFrames"));
}

static void testDeviceAgentManifest(nx::sdk::analytics::DeviceAgent* deviceAgent)
{
    nx::sdk::Error error = nx::sdk::Error::noError;
    const char* manifest = deviceAgent->manifest(&error);
    ASSERT_TRUE(manifest != nullptr);
    ASSERT_EQ(noError, (int) error);
    ASSERT_TRUE(manifest[0] != '\0');
    NX_PRINT << "DeviceAgent manifest:\n" << manifest;

    const std::string trimmedDeviceAgentManifest = trimString(manifest);
    ASSERT_EQ('{', trimmedDeviceAgentManifest.front());
    ASSERT_EQ('}', trimmedDeviceAgentManifest.back());

    deviceAgent->freeManifest(manifest);
}

static void testEngineSettings(nx::sdk::analytics::Engine* plugin)
{
    const nxpl::Setting settings[] = {
        {"setting1", "value1"},
        {"setting2", "value2"},
    };

    plugin->setSettings(nullptr, 0); //< Test assigning empty settings.
    plugin->setSettings(settings, 1); //< Test assigning a single setting.
    plugin->setSettings(settings, sizeof(settings) / sizeof(settings[0]));
}

static void testDeviceAgentSettings(nx::sdk::analytics::DeviceAgent* deviceAgent)
{
    const nxpl::Setting settings[] = {
        {"setting1", "value1"},
        {"setting2", "value2"},
    };

    deviceAgent->setSettings(nullptr, 0); //< Test assigning empty settings.
    deviceAgent->setSettings(settings, 1); //< Test assigning a single setting.
    deviceAgent->setSettings(settings, sizeof(settings) / sizeof(settings[0]));
}

class Action: public nx::sdk::analytics::Action
{
public:
    virtual void* queryInterface(const nxpl::NX_GUID& interfaceId) override
    {
        ASSERT_TRUE(false);
        return nullptr;
    }

    virtual unsigned int addRef() override { ASSERT_TRUE(false); return -1; }
    virtual unsigned int releaseRef() override { ASSERT_TRUE(false); return -1; }

    virtual const char* actionId() override { return m_actionId.c_str(); }
    virtual nxpl::NX_GUID objectId() override { return m_objectId; }
    virtual nxpl::NX_GUID deviceId() override { return m_deviceId; }
    virtual int64_t timestampUs() override { return m_timestampUs; }

    virtual const nxpl::Setting* params() override
    {
        return paramCount() == 0 ? nullptr : &m_params.front();
    }

    virtual int paramCount() override { return (int) m_params.size(); }

    virtual void handleResult(const char* actionUrl, const char* messageToUser) override
    {
        m_handleResultCalled = true;

        if (m_expectedNonNullActionUrl)
        {
            ASSERT_TRUE(actionUrl != nullptr);
            const std::string expectedUrlPrefix = "http://";
            ASSERT_EQ(0, std::string(actionUrl).compare(
                0, expectedUrlPrefix.size(), expectedUrlPrefix));
        }
        else
        {
            ASSERT_TRUE(actionUrl == nullptr);
        }

        if (m_expectedNonNullMessageToUser)
        {
            ASSERT_TRUE(messageToUser != nullptr);
            ASSERT_TRUE(messageToUser[0] != '\0');
        }
        else
        {
            ASSERT_TRUE(messageToUser == nullptr);
        }
    }

public:
    void assertExpectedState()
    {
        ASSERT_FALSE(
            (m_expectedNonNullActionUrl || m_expectedNonNullMessageToUser)
            && !m_handleResultCalled);
    }

    void setParams(const std::vector<std::pair<std::string, std::string>>& params)
    {
        m_paramsData = params;
        m_params.clear();
        for (const auto& param: m_paramsData)
            m_params.push_back({param.first.c_str(), param.second.c_str()});
    }

public:
    std::string m_actionId = "";
    nxpl::NX_GUID m_objectId = {{0}};
    nxpl::NX_GUID m_deviceId = {{0}};
    int64_t m_timestampUs = 0;
    bool m_handleResultCalled = false;
    bool m_expectedNonNullActionUrl = false;
    bool m_expectedNonNullMessageToUser = false;

private:
    std::vector<std::pair<std::string, std::string>> m_paramsData; //< Owns strings for m_params.
    std::vector<nxpl::Setting> m_params; //< Points to strings from m_paramsData.
};

static void testExecuteActionNonExisting(nx::sdk::analytics::Engine* plugin)
{
    Action action;
    action.m_actionId = "non_existing_actionId";

    nx::sdk::Error error = nx::sdk::Error::noError;
    plugin->executeAction(&action, &error);
    ASSERT_TRUE(error != nx::sdk::Error::noError);
    action.assertExpectedState();
}

static void testExecuteActionAddToList(nx::sdk::analytics::Engine* plugin)
{
    Action action;
    action.m_actionId = "nx.stub.addToList";
    action.setParams({{"paramA", "1"}, {"paramB", "2"}});
    action.m_expectedNonNullMessageToUser = true;

    nx::sdk::Error error = nx::sdk::Error::noError;
    plugin->executeAction(&action, &error);
    ASSERT_EQ(noError, (int) error);
    action.assertExpectedState();
}

static void testExecuteActionAddPerson(nx::sdk::analytics::Engine* plugin)
{
    Action action;
    action.m_actionId = "nx.stub.addPerson";
    action.m_objectId = {{0}};
    action.m_expectedNonNullActionUrl = true;

    nx::sdk::Error error = nx::sdk::Error::noError;
    plugin->executeAction(&action, &error);
    ASSERT_EQ(noError, (int) error);
    action.assertExpectedState();
}

class MetadataHandler: public nx::sdk::analytics::MetadataHandler
{
public:
    void handleMetadata(nx::sdk::Error error, nx::sdk::analytics::MetadataPacket* metadata) override
    {
        ASSERT_EQ(noError, (int) error);
        ASSERT_TRUE(metadata != nullptr);

        NX_PRINT << "MetadataHandler: Received metadata packet with timestamp "
            << metadata->timestampUsec() << " us, duration " << metadata->durationUsec() << " us";

        ASSERT_TRUE(metadata->durationUsec() >= 0);
        ASSERT_TRUE(metadata->timestampUsec() >= 0);
    }
};

class CompressedVideoFrame: public nx::sdk::analytics::DataPacket
{
public:
    virtual void* queryInterface(const nxpl::NX_GUID& interfaceId)
    {
        if (interfaceId == nx::sdk::analytics::IID_CompressedVideoPacket)
        {
            addRef();
            return this;
        }
        return nullptr;
    }

    virtual unsigned int addRef() { return ++m_refCounter; }
    virtual unsigned int releaseRef() { ASSERT_TRUE(m_refCounter > 1); return --m_refCounter; }

    virtual int64_t timestampUsec() const override { return 42; }

private:
    int m_refCounter = 1;
};

TEST(stub_analytics_plugin, test)
{
    nx::sdk::Error error = nx::sdk::Error::noError;

    nxpl::PluginInterface* const pluginObject = createNxAnalyticsPlugin();
    ASSERT_TRUE(pluginObject->queryInterface(nxpl::IID_Plugin3) != nullptr);
    pluginObject->releaseRef();

    const auto plugin = static_cast<nx::sdk::analytics::Plugin*>(
        pluginObject->queryInterface(nx::sdk::analytics::IID_Plugin));
    ASSERT_TRUE(plugin != nullptr);

    error = nx::sdk::Error::noError;
    nxpl::PluginInterface* const engineObject = plugin->createEngine(&error);
    ASSERT_EQ(noError, (int) error);

    const auto engine = static_cast<nx::sdk::analytics::Engine*>(
        engineObject->queryInterface(nx::sdk::analytics::IID_Engine));
    ASSERT_TRUE(engine != nullptr);

    ASSERT_EQ(plugin, engine->plugin());

    const std::string pluginName = engine->plugin()->name();
    ASSERT_TRUE(!pluginName.empty());
    NX_PRINT << "Plugin name: [" << pluginName << "]";
    ASSERT_STREQ(pluginName, "stub_analytics_plugin");

    testEngineManifest(engine);
    testEngineSettings(engine);
    testExecuteActionNonExisting(engine);
    testExecuteActionAddToList(engine);
    testExecuteActionAddPerson(engine);

    nx::sdk::DeviceInfo deviceInfo;
    error = nx::sdk::Error::noError;
    auto baseDeviceAgent = engine->obtainDeviceAgent(&deviceInfo, &error);
    ASSERT_EQ(noError, (int) error);
    ASSERT_TRUE(baseDeviceAgent != nullptr);
    ASSERT_TRUE(
        baseDeviceAgent->queryInterface(nx::sdk::analytics::IID_DeviceAgent) != nullptr);
    auto deviceAgent = static_cast<nx::sdk::analytics::ConsumingDeviceAgent*>(
        baseDeviceAgent->queryInterface(nx::sdk::analytics::IID_ConsumingDeviceAgent));
    ASSERT_TRUE(deviceAgent != nullptr);

    testDeviceAgentManifest(deviceAgent);
    testDeviceAgentSettings(deviceAgent);

    MetadataHandler metadataHandler;
    ASSERT_EQ(noError, (int) deviceAgent->setMetadataHandler(&metadataHandler));

    ASSERT_EQ(noError, (int) deviceAgent->startFetchingMetadata(
        /*typeList*/ nullptr, /*typeListSize*/ 0));

    CompressedVideoFrame compressedVideoFrame;
    ASSERT_EQ(noError, (int) deviceAgent->pushDataPacket(&compressedVideoFrame));

    ASSERT_EQ(noError, (int) deviceAgent->stopFetchingMetadata());

    deviceAgent->releaseRef();
    engine->releaseRef();
    plugin->releaseRef();
}

int main(int argc, const char* const argv[])
{
    return nx::kit::test::runAllTests("stub_analytics_plugin", argc, argv);
}
