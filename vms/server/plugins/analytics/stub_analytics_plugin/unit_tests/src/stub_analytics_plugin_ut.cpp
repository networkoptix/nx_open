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
struct RefCountableResultHolder
{
public:
    RefCountableResultHolder(Result<Value>&& result): result(std::move(result)) {}

    ~RefCountableResultHolder()
    {
        toPtr(result.value()); //< releaseRef
        toPtr(result.error().errorMessage()); //< releaseRef
    }

    bool isOk() const { return result.isOk(); }

    const Result<Value> result;
};

template<typename Value>
class ResultHolder
{
public:
    ResultHolder(Result<Value>&& result): result(std::move(result)) {}

    ~ResultHolder() { toPtr(result.error().errorMessage()); /*< releaseRef */ }

    bool isOk() const { return result.isOk(); }

    const Result<Value> result;
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
    const RefCountableResultHolder<const IString*> result{engine->manifest()};

    ASSERT_TRUE(result.isOk());
    const auto manifest = result.result.value();
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
    const RefCountableResultHolder<const IString*> result{deviceAgent->manifest()};
    ASSERT_TRUE(result.isOk());

    const auto manifest = result.result.value();
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
        const RefCountableResultHolder<const IStringMap*> result{
            engine->setSettings(makePtr<StringMap>().get())};
        ASSERT_TRUE(result.isOk());
    }

    {
        // Test assigning some settings
        const RefCountableResultHolder<const IStringMap*> result{
            engine->setSettings(settings.get())};
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
        const RefCountableResultHolder<const IStringMap*> result{
            deviceAgent->setSettings(makePtr<StringMap>().get())};
        ASSERT_TRUE(result.isOk());
    }

    {
        // Test assigning some settings.
        const RefCountableResultHolder<const IStringMap*> result{
            deviceAgent->setSettings(settings.get())};
        ASSERT_TRUE(result.isOk());
    }
    // TODO: Add test for assigning expected settings.
}

class Action: public RefCountable<IAction>
{
public:
    Action(): m_params(makePtr<StringMap>()) {}

    virtual const char* actionId() const override { return m_actionId.c_str(); }

    virtual int64_t timestampUs() const override { return m_timestampUs; }

    virtual const IStringMap* getParams() const override
    {
        if (!m_params)
            return nullptr;

        m_params->addRef();
        return m_params.get();
    }

    void setParams(const std::vector<std::pair<std::string, std::string>>& params)
    {
        m_params.reset(new StringMap());
        for (const auto& param: params)
            m_params->setItem(param.first, param.second);
    }

protected:
    virtual IObjectTrackInfo* getObjectTrackInfo() const override { return nullptr; }
    virtual void getObjectTrackId(Uuid* outValue) const override { *outValue = m_objectTrackId; }
    virtual void getDeviceId(Uuid* outValue) const override { *outValue = m_deviceId; }

public:
    std::string m_actionId;
    Uuid m_objectTrackId;
    Uuid m_deviceId;
    int64_t m_timestampUs = 0;

private:
    Ptr<StringMap> m_params;
};

enum class ExpectedActionResult
{
    error,
    actionUrl,
    messageToUser,
};

template<class RefCountableString>
static bool stringIsNullOrEmpty(int line, const RefCountableString& s)
{
    ASSERT_FALSE_AT_LINE(line, s && !s->str()); //< IString::str() contract violated.

    return s == nullptr || s->str()[0] == '\0';
}

static void testExecuteAction(
    int line,
    IEngine* engine,
    Ptr<const IAction> action,
    ExpectedActionResult expectedActionResult)
{
    const ResultHolder<IAction::Result> resultHolder{engine->executeAction(action.get())};

    const auto& value = resultHolder.result.value();
    const auto& error = resultHolder.result.error();

    switch (expectedActionResult)
    {
        case ExpectedActionResult::error:
            ASSERT_FALSE_AT_LINE(line, resultHolder.isOk());

            // Testing nx_sdk internal consistency.
            ASSERT_FALSE_AT_LINE(line, error.errorCode() == ErrorCode::noError);
            ASSERT_FALSE_AT_LINE(line, stringIsNullOrEmpty(line, error.errorMessage()));
            ASSERT_FALSE_AT_LINE(line, value.actionUrl);
            ASSERT_FALSE_AT_LINE(line, value.messageToUser);
            break;

        case ExpectedActionResult::actionUrl:
        {
            ASSERT_TRUE_AT_LINE(line, resultHolder.isOk());

            ASSERT_FALSE_AT_LINE(line, stringIsNullOrEmpty(line, value.actionUrl));
            static const std::string kExpectedUrlPrefix = "http://";
            ASSERT_EQ_AT_LINE(line, 0, std::string(value.actionUrl->str()).compare(
                0, kExpectedUrlPrefix.size(), kExpectedUrlPrefix));

            ASSERT_TRUE_AT_LINE(line, stringIsNullOrEmpty(line, value.messageToUser));

            // Testing nx_sdk internal consistency.
            ASSERT_TRUE_AT_LINE(line, error.errorCode() == ErrorCode::noError);
            ASSERT_FALSE_AT_LINE(line, error.errorMessage());
            break;
        }

        case ExpectedActionResult::messageToUser:
            ASSERT_TRUE_AT_LINE(line, resultHolder.isOk());

            ASSERT_TRUE_AT_LINE(line, stringIsNullOrEmpty(line, value.actionUrl));

            ASSERT_FALSE_AT_LINE(line, stringIsNullOrEmpty(line, value.messageToUser));

            // Testing nx_sdk internal consistency.
            ASSERT_TRUE_AT_LINE(line, error.errorCode() == ErrorCode::noError);
            ASSERT_FALSE_AT_LINE(line, error.errorMessage());
            break;

        default:
            ASSERT_TRUE(false); //< Internal error.
    }
}
#define TEST_EXECUTE_ACTION(...) testExecuteAction(__LINE__, __VA_ARGS__)

static void testExecuteActionNonExisting(IEngine* engine)
{
    const auto action = makePtr<Action>();
    action->m_actionId = "non_existing_actionId";

    TEST_EXECUTE_ACTION(engine, action, ExpectedActionResult::error);
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

    TEST_EXECUTE_ACTION(engine, action, ExpectedActionResult::messageToUser);
}

static void testExecuteActionAddPerson(IEngine* engine)
{
    const auto action = makePtr<Action>();
    action->m_actionId = "nx.stub.addPerson";
    action->m_objectTrackId = Uuid();

    TEST_EXECUTE_ACTION(engine, action, ExpectedActionResult::actionUrl);
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

    const RefCountableResultHolder<IEngine*> result{plugin->createEngine()};
    ASSERT_TRUE(result.isOk());

    const auto engine = result.result.value();
    ASSERT_TRUE(engine);
    ASSERT_TRUE(engine->queryInterface<IEngine>());

    testEngineManifest(engine);
    testEngineSettings(engine);

    testExecuteActionNonExisting(engine);
    testExecuteActionAddToList(engine);
    testExecuteActionAddPerson(engine);

    const auto deviceInfo = makePtr<DeviceInfo>();
    deviceInfo->setId("TEST");
    const RefCountableResultHolder<IDeviceAgent*> deviceAgentResult{
        engine->obtainDeviceAgent(deviceInfo.get())};
    ASSERT_TRUE(deviceAgentResult.isOk());

    const auto baseDeviceAgent = deviceAgentResult.result.value();
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
    const ResultHolder<void> setNeededMetadataTypesResult{
        deviceAgent->setNeededMetadataTypes(metadataTypes.get())};
    ASSERT_TRUE(setNeededMetadataTypesResult.isOk());

    const auto compressedVideoPacket = makePtr<CompressedVideoPacket>();
    const ResultHolder<void> pushDataPacketResult{
        deviceAgent->pushDataPacket(compressedVideoPacket.get())};
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
