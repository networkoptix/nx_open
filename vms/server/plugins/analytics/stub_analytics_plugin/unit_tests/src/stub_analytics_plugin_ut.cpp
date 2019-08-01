// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include <vector>

#include <nx/kit/debug.h>
#include <nx/kit/test.h>

#include <nx/sdk/interface.h>
#include <nx/sdk/helpers/ref_countable.h>
#include <nx/sdk/helpers/to_string.h>
#include <nx/sdk/ptr.h>
#include <nx/sdk/helpers/string_map.h>
#include <nx/sdk/helpers/device_info.h>
#include <nx/sdk/helpers/string.h>
#include <nx/sdk/helpers/error.h>
#include <nx/sdk/analytics/helpers/metadata_types.h>
#include <nx/sdk/uuid.h>
#include <nx/sdk/i_utility_provider.h>

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

template<typename Value>
struct ResultHolder
{
public:
    ResultHolder(Result<Value>&& result): result(std::move(result)) {};
    ~ResultHolder()
    {
        toPtr(result.value);
        toPtr(result.error.errorMessage);
    }

    bool isOk() const { return result.isOk(); }

    const Result<Value> result;
};

class VoidResultHolder
{
public:
    VoidResultHolder(Result<void>&& result): result(std::move(result)) {};
    ~VoidResultHolder() { toPtr(result.error.errorMessage); }

    bool isOk() const { return result.isOk(); }

    const Result<void> result;
};

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
    const ResultHolder<const IString*> result = engine->manifest();

    ASSERT_TRUE(result.isOk());
    const auto manifest = result.result.value;
    ASSERT_TRUE(manifest);

    const char* const manifestStr = manifest->str();
    ASSERT_TRUE(manifestStr);
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
    const ResultHolder<const IString*> result = deviceAgent->manifest();
    ASSERT_TRUE(result.isOk());

    const auto manifest = result.result.value;
    ASSERT_TRUE(manifest);
    const char* manifestStr = manifest->str();
    ASSERT_TRUE(manifestStr != nullptr);
    ASSERT_TRUE(manifestStr[0] != '\0');
    NX_PRINT << "DeviceAgent manifest:\n" << manifest;

    const std::string trimmedDeviceAgentManifest = trimString(manifestStr);
    ASSERT_EQ('{', trimmedDeviceAgentManifest.front());
    ASSERT_EQ('}', trimmedDeviceAgentManifest.back());
}

static void testEngineSettings(IEngine* engine)
{
    const auto settings = makePtr<StringMap>();
    settings->setItem("setting1", "value1");
    settings->setItem("setting2", "value2");

    {
        // Test assigning empty settings.
        const ResultHolder<const IStringMap*> result =
            engine->setSettings(makePtr<StringMap>().get());
        ASSERT_TRUE(result.isOk());
    }

    {
        // Test assigning some settings
        const ResultHolder<const IStringMap*> result =
            engine->setSettings(settings.get());
        ASSERT_TRUE(result.isOk());
    }
}

static void testDeviceAgentSettings(IDeviceAgent* deviceAgent)
{
    const auto settings = makePtr<StringMap>();
    settings->setItem("setting1", "value1");
    settings->setItem("setting2", "value2");

    {
        // Test assigning empty settings.
        const ResultHolder<const IStringMap*> result =
            deviceAgent->setSettings(makePtr<StringMap>().get());
        ASSERT_TRUE(result.isOk());
    }

    {
        // Test assigning some settings.
        const ResultHolder<const IStringMap*> result =
            deviceAgent->setSettings(settings.get());
        ASSERT_TRUE(result.isOk());
    }
    // TODO: Add test for assigning expected settings.
}

class Action: public RefCountable<IAction>
{
public:
    Action(): m_params(makePtr<StringMap>()) {}

    virtual const char* actionId() const override { return m_actionId.c_str(); }
    virtual Uuid objectTrackId() const override { return m_objectTrackId; }
    virtual Uuid deviceId() const override { return m_deviceId; }
    virtual int64_t timestampUs() const override { return m_timestampUs; }

    virtual const IStringMap* getParams() const override
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
            m_params->setItem(param.first, param.second);
    }

protected:
    virtual IObjectTrackInfo* getObjectTrackInfo() const override { return nullptr; }

public:
    std::string m_actionId;
    Uuid m_objectTrackId;
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

    const VoidResultHolder result = engine->executeAction(action.get());
    ASSERT_TRUE(!result.isOk());
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

    const VoidResultHolder result = engine->executeAction(action.get());
    ASSERT_TRUE(result.isOk());
    action->assertExpectedState();
}

static void testExecuteActionAddPerson(IEngine* engine)
{
    const auto action = makePtr<Action>();
    action->m_actionId = "nx.stub.addPerson";
    action->m_objectTrackId = Uuid();
    action->m_expectedNonNullActionUrl = true;

    const VoidResultHolder result = engine->executeAction(action.get());
    ASSERT_TRUE(result.isOk());
    action->assertExpectedState();
}

class DeviceAgentHandler: public nx::sdk::RefCountable<IDeviceAgent::IHandler>
{
public:
    virtual void handleMetadata(IMetadataPacket* metadata) override
    {
        ASSERT_TRUE(metadata != nullptr);

        NX_PRINT << "DeviceAgentHandler: Received metadata packet with timestamp "
            << metadata->timestampUs() << " us";

        ASSERT_TRUE(metadata->timestampUs() >= 0);
    }

    virtual void handlePluginDiagnosticEvent(IPluginDiagnosticEvent* event) override
    {
        NX_PRINT << "DeviceAgentHandler: Received a plugin diagnostic event: "
            << "level " << (int) event->level() << ", "
            << "caption " << nx::kit::utils::toString(event->caption()) << ", "
            << "description " << nx::kit::utils::toString(event->description());
    }
};

class EngineHandler: public nx::sdk::RefCountable<IEngine::IHandler>
{
public:
    virtual void handlePluginDiagnosticEvent(IPluginDiagnosticEvent* event) override
    {
        NX_PRINT << "EngineHandler: Received a plugin diagnostic event: "
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
    virtual MediaFlags flags() const override { return MediaFlags::none; }

    virtual int width() const override { return 256; }
    virtual int height() const override { return 128; }

protected:
    virtual const IMediaContext* getContext() const override { return nullptr; }

private:
    const std::vector<char> m_data = std::vector<char>(width() * height(), /*dummy*/ 42);
};

class UtilityProvider: public RefCountable<IUtilityProvider>
{
public:
    virtual int64_t vmsSystemTimeSinceEpochMs() const override { return 0; }
    virtual const nx::sdk::IString* getHomeDir() const override { return new nx::sdk::String(); }
};

class Handler: public nx::sdk::RefCountable<nx::sdk::analytics::IDeviceAgent::IHandler>
{
public:
    virtual void handleMetadata(IMetadataPacket* metadataPacket) override
    {
        ASSERT_TRUE(metadataPacket);
    }

    virtual void handlePluginDiagnosticEvent(IPluginDiagnosticEvent* event) override
    {
        ASSERT_TRUE(event);
    }
};

TEST(stub_analytics_plugin, test)
{
    // These handlers should be destroyed after the Plugin, Engine and DeviceAgent objects.
    const auto engineHandler = nx::sdk::makePtr<EngineHandler>();
    const auto deviceAgentHandler = nx::sdk::makePtr<DeviceAgentHandler>();

    const auto pluginObject = toPtr(createNxPlugin());
    ASSERT_TRUE(pluginObject);
    ASSERT_TRUE(pluginObject->queryInterface<IRefCountable>());
    ASSERT_TRUE(pluginObject->queryInterface<nx::sdk::IPlugin>());
    const auto plugin = pluginObject->queryInterface<nx::sdk::analytics::IPlugin>();
    ASSERT_TRUE(plugin);

    plugin->setUtilityProvider(makePtr<UtilityProvider>().get());

    const ResultHolder<IEngine*> result = plugin->createEngine();
    ASSERT_TRUE(result.isOk());

    const auto engine = result.result.value;
    ASSERT_TRUE(engine);
    ASSERT_TRUE(engine->queryInterface<IEngine>());

    ASSERT_EQ(plugin.get(), engine->plugin());

    testEngineManifest(engine);
    testEngineSettings(engine);

    testExecuteActionNonExisting(engine);
    testExecuteActionAddToList(engine);
    testExecuteActionAddPerson(engine);

    const auto deviceInfo = makePtr<DeviceInfo>();
    deviceInfo->setId("TEST");
    const ResultHolder<IDeviceAgent*> deviceAgentResult = engine->obtainDeviceAgent(deviceInfo.get());
    ASSERT_TRUE(deviceAgentResult.isOk());

    const auto baseDeviceAgent = deviceAgentResult.result.value;
    ASSERT_TRUE(baseDeviceAgent);
    ASSERT_TRUE(baseDeviceAgent->queryInterface<IDeviceAgent>());
    const auto deviceAgent = baseDeviceAgent->queryInterface<IConsumingDeviceAgent>();
    ASSERT_TRUE(deviceAgent);

    deviceAgent->setHandler(makePtr<DeviceAgentHandler>().get());
    testDeviceAgentManifest(deviceAgent.get());
    testDeviceAgentSettings(deviceAgent.get());

    const auto handler = makePtr<Handler>();
    deviceAgent->setHandler(handler.get());

    const auto metadataTypes = makePtr<MetadataTypes>();
    const VoidResultHolder setNeededMetadataTypesResult =
        deviceAgent->setNeededMetadataTypes(metadataTypes.get());
    ASSERT_TRUE(setNeededMetadataTypesResult.isOk());

    const auto compressedVideoPacket = makePtr<CompressedVideoPacket>();
    const VoidResultHolder pushDataPacketResult =
        deviceAgent->pushDataPacket(compressedVideoPacket.get());
    ASSERT_TRUE(pushDataPacketResult.isOk());
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
