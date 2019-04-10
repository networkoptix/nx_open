#include <iostream>
#include <vector>

#include <nx/kit/debug.h>
#include <nx/kit/test.h>

#include <nx/sdk/interface.h>
#include <nx/sdk/helpers/ref_countable.h>
#include <nx/sdk/helpers/to_string.h>
#include <nx/sdk/helpers/ptr.h>
#include <nx/sdk/helpers/string_map.h>
#include <nx/sdk/helpers/device_info.h>
#include <nx/sdk/analytics/helpers/metadata_types.h>
#include <nx/sdk/uuid.h>

#include <nx/sdk/analytics/i_consuming_device_agent.h>
#include <nx/sdk/analytics/i_uncompressed_video_frame.h>
#include <nx/sdk/analytics/i_compressed_video_packet.h>
#include <nx/sdk/analytics/i_engine.h>
#include <nx/sdk/analytics/i_plugin.h>

extern "C" nx::sdk::IPlugin* createNxPlugin();

namespace nx {
namespace vms_server_plugins {
namespace analytics {
namespace stub {
namespace test {

using namespace nx::sdk;
using namespace nx::sdk::analytics;

static std::string trimString(const std::string& s)
{
    int start = 0;
    while (start < (int) s.size() && s.at(start) <= ' ')
        ++start;
    int end = (int) s.size() - 1;
    while (end >= 0 && s.at(end) <= ' ')
        --end;
    if (end < start)
        return "";
    return s.substr(start, end - start + 1);
}

static void testEngineManifest(IEngine* engine)
{
    Error error = Error::noError;

    const auto manifest = toPtr(engine->manifest(&error));

    ASSERT_TRUE(manifest);
    const char* manifestStr = manifest->str();

    ASSERT_TRUE(manifestStr != nullptr);
    ASSERT_EQ(Error::noError, error);
    ASSERT_TRUE(manifestStr[0] != '\0');
    NX_PRINT << "Engine manifest:\n" << manifestStr;

    const std::string trimmedEngineManifest = trimString(manifestStr);
    ASSERT_EQ('{', trimmedEngineManifest.front());
    ASSERT_EQ('}', trimmedEngineManifest.back());

    // This test assumes that the plugin consumes compressed frames - verify it in the manifest.
    ASSERT_EQ(std::string::npos, std::string(manifestStr).find("needUncompressedVideoFrames"));
}

static void testDeviceAgentManifest(IDeviceAgent* deviceAgent)
{
    Error error = Error::noError;
    const auto manifest = toPtr(deviceAgent->manifest(&error));

    ASSERT_TRUE(manifest);
    const char* manifestStr = manifest->str();
    ASSERT_TRUE(manifestStr != nullptr);
    ASSERT_EQ(Error::noError, error);
    ASSERT_TRUE(manifestStr[0] != '\0');
    NX_PRINT << "DeviceAgent manifest:\n" << manifest;

    const std::string trimmedDeviceAgentManifest = trimString(manifestStr);
    ASSERT_EQ('{', trimmedDeviceAgentManifest.front());
    ASSERT_EQ('}', trimmedDeviceAgentManifest.back());
}

static void testEngineSettings(IEngine* engine)
{
    const auto settings = makePtr<StringMap>();
    settings->addItem("setting1", "value1");
    settings->addItem("setting2", "value2");

    engine->setSettings(nullptr); //< Test assigning null settings.
    engine->setSettings(makePtr<StringMap>().get()); //< Test assigning empty settings.
    engine->setSettings(settings.get()); //< Test assigning some settings.
}

static void testDeviceAgentSettings(IDeviceAgent* deviceAgent)
{
    const auto settings = makePtr<StringMap>();
    settings->addItem("setting1", "value1");
    settings->addItem("setting2", "value2");

    deviceAgent->setSettings(nullptr); //< Test assigning null settings.
    deviceAgent->setSettings(makePtr<StringMap>().get()); //< Test assigning empty settings.
    deviceAgent->setSettings(settings.get()); //< Test assigning some settings.
}

class Action: public RefCountable<IAction>
{
public:
    Action(): m_params(new StringMap()) {}

    virtual const char* actionId() const override { return m_actionId.c_str(); }
    virtual Uuid objectId() const override { return m_objectId; }
    virtual Uuid deviceId() const override { return m_deviceId; }
    virtual IObjectTrackInfo* objectTrackInfo() const override { return nullptr; }
    virtual int64_t timestampUs() const override { return m_timestampUs; }

    virtual const IStringMap* params() const override
    {
        if (!m_params)
            return nullptr;

        m_params->addRef();
        return m_params.get();
    }

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
    void assertExpectedState() const
    {
        ASSERT_FALSE(
            (m_expectedNonNullActionUrl || m_expectedNonNullMessageToUser)
            && !m_handleResultCalled);
    }

    void setParams(const std::vector<std::pair<std::string, std::string>>& params)
    {
        m_params.reset(new StringMap());
        for (const auto& param: params)
            m_params->addItem(param.first, param.second);
    }

public:
    std::string m_actionId;
    Uuid m_objectId;
    Uuid m_deviceId;
    int64_t m_timestampUs = 0;
    bool m_handleResultCalled = false;
    bool m_expectedNonNullActionUrl = false;
    bool m_expectedNonNullMessageToUser = false;

private:
    Ptr<StringMap> m_params;
};

static void testExecuteActionNonExisting(IEngine* engine)
{
    auto action = makePtr<Action>();
    action->m_actionId = "non_existing_actionId";

    Error error = Error::noError;
    engine->executeAction(action.get(), &error);
    ASSERT_TRUE(error != Error::noError);
    action->assertExpectedState();
}

static void testExecuteActionAddToList(IEngine* engine)
{
    const auto action = makePtr<Action>();
    action->m_actionId = "nx.stub.addToList";
    action->setParams({
        {"testTextField", "Some text"},
        {"testSpinBox", "20"},
        {"testDoubleSpinBox", "13.5"},
        {"testComboBox", "value1"},
        {"testCheckBox", "false"}
    });
    action->m_expectedNonNullMessageToUser = true;

    Error error = Error::noError;
    engine->executeAction(action.get(), &error);
    ASSERT_EQ(Error::noError, error);
    action->assertExpectedState();
}

static void testExecuteActionAddPerson(IEngine* engine)
{
    const auto action = makePtr<Action>();
    action->m_actionId = "nx.stub.addPerson";
    action->m_objectId = Uuid();
    action->m_expectedNonNullActionUrl = true;

    Error error = Error::noError;
    engine->executeAction(action.get(), &error);
    ASSERT_EQ(Error::noError, error);
    action->assertExpectedState();
}

class DeviceAgentHandler: public IDeviceAgent::IHandler
{
public:
    virtual void handleMetadata(IMetadataPacket* metadata) override
    {
        ASSERT_TRUE(metadata != nullptr);

        NX_PRINT << "DeviceAgentHandler: Received metadata packet with timestamp "
            << metadata->timestampUs() << " us, duration " << metadata->durationUs() << " us";

        ASSERT_TRUE(metadata->durationUs() >= 0);
        ASSERT_TRUE(metadata->timestampUs() >= 0);
    }

    virtual void handlePluginEvent(IPluginEvent* event) override
    {
        NX_PRINT << "DeviceAgentHandler: Received plugin event: "
            << "level " << (int) event->level() << ", "
            << "caption " << nx::kit::utils::toString(event->caption()) << ", "
            << "description " << nx::kit::utils::toString(event->description());
    }
};

class EngineHandler: public IEngine::IHandler
{
public:
    virtual void handlePluginEvent(IPluginEvent* event) override
    {
        NX_PRINT << "EngineHandler: Received plugin event: "
            << "level " << (int) event->level() << ", "
            << "caption " << nx::kit::utils::toString(event->caption()) << ", "
            << "description " << nx::kit::utils::toString(event->description());
    }
};

class CompressedVideoPacket: public RefCountable<ICompressedVideoPacket>
{
public:
    virtual int64_t timestampUs() const override { return /*dummy*/ 42; }

    virtual const char* codec() const override { return "test_stub_codec"; }
    virtual const char* data() const override { return m_data.data(); }
    virtual int dataSize() const override { return (int) m_data.size(); }
    virtual const IMediaContext* context() const override { return nullptr; }
    virtual MediaFlags flags() const override { return MediaFlags::none; }

    virtual int width() const override { return 256; }
    virtual int height() const override { return 128; }

private:
    const std::vector<char> m_data = std::vector<char>(width() * height(), /*dummy*/ 42);
};

TEST(stub_analytics_plugin, test)
{
    // These handlers should be destroyed after the Plugin, Engine and DeviceAgent objects.
    const auto engineHandler = std::make_unique<EngineHandler>();
    const auto deviceAgentHandler = std::make_unique<DeviceAgentHandler>();

    Error error = Error::noError;

    const auto pluginObject = toPtr(createNxPlugin());
    ASSERT_TRUE(pluginObject);
    ASSERT_TRUE(queryInterfacePtr<IRefCountable>(pluginObject));
    ASSERT_TRUE(queryInterfacePtr<nx::sdk::IPlugin>(pluginObject));
    const auto plugin = queryInterfacePtr<nx::sdk::analytics::IPlugin>(pluginObject);
    ASSERT_TRUE(plugin);

    const auto engine = toPtr(plugin->createEngine(&error));
    ASSERT_EQ(Error::noError, error);
    ASSERT_TRUE(engine);
    ASSERT_TRUE(queryInterfacePtr<IEngine>(engine));

    ASSERT_EQ(plugin.get(), engine->plugin());

    ASSERT_EQ(Error::noError, engine->setHandler(engineHandler.get()));

    const std::string pluginName = engine->plugin()->name();
    ASSERT_TRUE(!pluginName.empty());
    NX_PRINT << "Plugin name: [" << pluginName << "]";
    ASSERT_STREQ(pluginName, "stub_analytics_plugin");

    testEngineManifest(engine.get());
    testEngineSettings(engine.get());

    testExecuteActionNonExisting(engine.get());
    testExecuteActionAddToList(engine.get());
    testExecuteActionAddPerson(engine.get());

    const auto deviceInfo = makePtr<DeviceInfo>();
    deviceInfo->setId("TEST");
    const auto baseDeviceAgent = toPtr(engine->obtainDeviceAgent(deviceInfo.get(), &error));
    ASSERT_EQ(Error::noError, error);
    ASSERT_TRUE(baseDeviceAgent);
    ASSERT_TRUE(queryInterfacePtr<IDeviceAgent>(baseDeviceAgent));
    const auto deviceAgent = queryInterfacePtr<IConsumingDeviceAgent>(baseDeviceAgent);
    ASSERT_TRUE(deviceAgent);

    ASSERT_EQ(Error::noError, deviceAgent->setHandler(deviceAgentHandler.get()));

    testDeviceAgentManifest(deviceAgent.get());
    testDeviceAgentSettings(deviceAgent.get());

    const auto metadataTypes = makePtr<MetadataTypes>();
    ASSERT_EQ(Error::noError, deviceAgent->setNeededMetadataTypes(metadataTypes.get()));

    const auto compressedVideoPacket = makePtr<CompressedVideoPacket>();
    ASSERT_EQ(Error::noError, deviceAgent->pushDataPacket(compressedVideoPacket.get()));
}

} // namespace test
} // namespace stub
} // namespace analytics
} // namespace vms_server_plugins
} // namespace nx

int main()
{
    return nx::kit::test::runAllTests("stub_analytics_plugin");
}
