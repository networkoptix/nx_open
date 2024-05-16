// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include <fstream>
#include <string>
#include <vector>

#if defined(_WIN32)
    // Needed for UTF-8 -> UTF-16 conversion for WinAPI calls on filenames.
    #include <codecvt>
    #include <iostream>
    #include <locale>

    #include <windows.h>
#else
    #include <dlfcn.h>
    #include <unistd.h>
#endif

#include <nx/kit/debug.h>
#include <nx/kit/json.h>
#include <nx/kit/test.h>
#include <nx/sdk/analytics/helpers/metadata_types.h>
#include <nx/sdk/analytics/i_compressed_video_packet.h>
#include <nx/sdk/analytics/i_consuming_device_agent.h>
#include <nx/sdk/analytics/i_engine.h>
#include <nx/sdk/analytics/i_plugin.h>
#include <nx/sdk/analytics/i_uncompressed_video_frame.h>
#include <nx/sdk/helpers/device_info.h>
#include <nx/sdk/helpers/error.h>
#include <nx/sdk/helpers/lib_context.h>
#include <nx/sdk/helpers/ref_countable.h>
#include <nx/sdk/helpers/settings_response.h>
#include <nx/sdk/helpers/string.h>
#include <nx/sdk/helpers/string_map.h>
#include <nx/sdk/helpers/to_string.h>
#include <nx/sdk/i_utility_provider.h>
#include <nx/sdk/interface.h>
#include <nx/sdk/ptr.h>
#include <nx/sdk/uuid.h>

namespace nx {
namespace vms_server_plugins {
namespace analytics {
namespace test {

using namespace nx::sdk;
using namespace nx::sdk::analytics;

std::vector<std::string> g_pluginLibFilenames; //< Paths of plugin libraries to test.
int g_pluginInstanceIndex = -1; //< If not -1, only this instance will be tested.

template<typename Value>
struct RefCountableResultHolder
{
public:
    RefCountableResultHolder(Result<Value>&& result): result(std::move(result)) {}

    ~RefCountableResultHolder()
    {
        Ptr(result.value()); //< releaseRef
        Ptr(result.error().errorMessage()); //< releaseRef
    }

    bool isOk() const { return result.isOk(); }

    const Result<Value> result;
};

template<typename Value>
class ResultHolder
{
public:
    ResultHolder(Result<Value>&& result): result(std::move(result)) {}

    ~ResultHolder()
    {
        Ptr(result.error().errorMessage()); //< releaseRef
    }

    bool isOk() const { return result.isOk(); }

    const Result<Value> result;
};

// TODO: When manifest is introduced for any IPlugin (not only Analytics), rewrite.
static void testAnalyticsPluginManifest(nx::sdk::analytics::IPlugin* plugin)
{
    const RefCountableResultHolder<const IString*> result{plugin->manifest()};

    ASSERT_TRUE(result.isOk());
    const auto pluginManifest = result.result.value();
    ASSERT_TRUE(pluginManifest);

    const char* const pluginManifestStr = pluginManifest->str();
    ASSERT_TRUE(pluginManifestStr);
    ASSERT_TRUE(pluginManifestStr[0] != '\0');
    NX_PRINT << "Plugin manifest:\n" << pluginManifestStr;

    std::string pluginManifestError;
    const nx::kit::Json pluginManifestJson =
        nx::kit::Json::parse(pluginManifest->str(), pluginManifestError);
    ASSERT_EQ("", pluginManifestError);
    ASSERT_TRUE(pluginManifestJson.is_object());
}

static void testEngineManifest(IEngine* engine)
{
    const RefCountableResultHolder<const IString*> result{engine->manifest()};

    ASSERT_TRUE(result.isOk());
    const auto engineManifest = result.result.value();
    ASSERT_TRUE(engineManifest);

    const char* const engineManifestStr = engineManifest->str();
    ASSERT_TRUE(engineManifestStr);
    ASSERT_TRUE(engineManifestStr[0] != '\0');
    NX_PRINT << "Engine manifest:\n" << engineManifestStr;

    std::string engineManifestError;
    const nx::kit::Json engineManifestJson =
        nx::kit::Json::parse(engineManifest->str(), engineManifestError);
    ASSERT_EQ("", engineManifestError);
    ASSERT_TRUE(engineManifestJson.is_object());
}

static void testDeviceAgentManifest(IDeviceAgent* deviceAgent)
{
    const RefCountableResultHolder<const IString*> result{deviceAgent->manifest()};
    ASSERT_TRUE(result.isOk());

    const auto deviceAgentManifest = result.result.value();
    ASSERT_TRUE(deviceAgentManifest);
    const char* deviceAgentManifestStr = deviceAgentManifest->str();
    ASSERT_TRUE(deviceAgentManifestStr);
    ASSERT_TRUE(deviceAgentManifestStr[0] != '\0');
    NX_PRINT << "DeviceAgent manifest:\n" << deviceAgentManifest;

    std::string deviceAgentManifestError;
    const nx::kit::Json deviceAgentManifestJson =
        nx::kit::Json::parse(deviceAgentManifest->str(), deviceAgentManifestError);
    ASSERT_EQ("", deviceAgentManifestError);
    ASSERT_TRUE(deviceAgentManifestJson.is_object());
}

static void testEngineSettings(IEngine* engine)
{
    const auto settings = makePtr<StringMap>();
    settings->setItem("setting1", "value1");
    settings->setItem("setting2", "value2");

    {
        // Test assigning empty settings.
        const RefCountableResultHolder<const ISettingsResponse*> result{
            engine->setSettings(makePtr<StringMap>().get())};
        ASSERT_TRUE(result.isOk());
    }

    {
        // Test assigning some settings
        const RefCountableResultHolder<const ISettingsResponse*> result{
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
        const RefCountableResultHolder<const ISettingsResponse*> result{
            deviceAgent->setSettings(makePtr<StringMap>().get())};
        ASSERT_TRUE(result.isOk());
    }

    {
        // Test assigning some settings.
        const RefCountableResultHolder<const ISettingsResponse*> result{
            deviceAgent->setSettings(settings.get())};
        ASSERT_TRUE(result.isOk());
    }
}

class DeviceAgentHandler: public nx::sdk::RefCountable<IDeviceAgent::IHandler>
{
public:
    virtual void handleMetadata(IMetadataPacket* metadata) override
    {
        ASSERT_TRUE(metadata);

        NX_PRINT << "DeviceAgentHandler: Received metadata packet with timestamp "
            << metadata->timestampUs() << " us";

        ASSERT_TRUE(metadata->timestampUs() >= 0);
    }

    virtual void handlePluginDiagnosticEvent(IPluginDiagnosticEvent* event) override
    {
        ASSERT_TRUE(event);

        NX_PRINT << "DeviceAgentHandler: Received a plugin diagnostic event: "
            << "level " << (int) event->level() << ", "
            << "caption " << nx::kit::utils::toString(event->caption()) << ", "
            << "description " << nx::kit::utils::toString(event->description());
    }

    virtual void pushManifest(const IString* manifest) override
    {
        ASSERT_TRUE(manifest);
        ASSERT_TRUE(manifest->str());

        NX_PRINT << "DeviceAgentHandler: Received a pushed manifest:\n"
            << manifest->str();
    }
};

class EngineHandler: public nx::sdk::RefCountable<IEngine::IHandler>
{
public:
    virtual void handlePluginDiagnosticEvent(IPluginDiagnosticEvent* event) override
    {
        ASSERT_TRUE(event);

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

protected:
    virtual nx::sdk::IList<nx::sdk::analytics::IMetadataPacket>* getMetadataList() const override
    {
        return nullptr;
    }

private:
    const std::vector<char> m_data = std::vector<char>(width() * height(), /*dummy*/ 42);
};

class UtilityProvider: public RefCountable<IUtilityProvider>
{
public:
    virtual int64_t vmsSystemTimeSinceEpochMs() const override { return 0; }
    virtual const nx::sdk::IString* getHomeDir() const override { return new nx::sdk::String(); }
    virtual const char* serverId() const override { return ""; }
    virtual IString* cloudSystemId() const { return new nx::sdk::String(); }
    virtual IString* cloudAuthKey() const { return new nx::sdk::String(); }

    virtual void doSendHttpRequest(
        HttpDomainName requestDomain,
        const char* url,
        const char* httpMethod,
        const char* mimeType,
        const char* requestBody,
        IHttpRequestCompletionHandler* callback) const override
    {
        NX_PRINT << "Making HTTP request with HttpDomainName = " << (int) requestDomain
                 << ", url = " << url << ", httpMethod = " << httpMethod << ", mimeType = "
                 << mimeType << ", requestBody = " << requestBody;
        callback->execute({new nx::sdk::String()});
    }

    virtual const nx::sdk::IString* getServerSdkVersion() const override
    {
        return new nx::sdk::String(sdkVersion());
    }
};

/**
 * Any Analytics Plugin similar to a stub must pass. It means:
 * - The Plugin creates a DeviceAgent for any DeviceInfo.
 * - The DeviceAgent accepts compressed video frames, thus supports IConsumingDeviceAgent.
 * - The DeviceAgent accepts unknown Settings, silently ignoring them without producing an error.
 * - The DeviceAgent accepts any needed-metadata-types.
 */
static void testStubAnalyticsPluginEngine(IEngine* engine)
{
    const auto deviceInfo = makePtr<DeviceInfo>();
    deviceInfo->setId("TEST");
    const RefCountableResultHolder<IDeviceAgent*> obtainDeviceAgentResult{
        engine->obtainDeviceAgent(deviceInfo.get())};
    ASSERT_TRUE(obtainDeviceAgentResult.isOk());

    const auto deviceAgent = obtainDeviceAgentResult.result.value();
    ASSERT_TRUE(deviceAgent);
    ASSERT_TRUE(deviceAgent->queryInterface<IDeviceAgent>());
    const auto consumingDeviceAgent = deviceAgent->queryInterface<IConsumingDeviceAgent>();
    ASSERT_TRUE(consumingDeviceAgent);

    consumingDeviceAgent->setHandler(nx::sdk::makePtr<DeviceAgentHandler>().get());
    testDeviceAgentManifest(consumingDeviceAgent.get());
    testDeviceAgentSettings(consumingDeviceAgent.get());

    const ResultHolder<void> setNeededMetadataTypesResult{
        consumingDeviceAgent->setNeededMetadataTypes(makePtr<MetadataTypes>().get())};
    ASSERT_TRUE(setNeededMetadataTypesResult.isOk());

    const ResultHolder<void> pushDataPacketResult{
        consumingDeviceAgent->pushDataPacket(makePtr<CompressedVideoPacket>().get())};
    ASSERT_TRUE(pushDataPacketResult.isOk());
}

/** Any Analytics Plugin must pass. */
static void testAnalyticsPlugin(nx::sdk::analytics::IPlugin* plugin)
{
    ASSERT_TRUE(plugin);

    testAnalyticsPluginManifest(plugin);

    const RefCountableResultHolder<IEngine*> createEngineResult{plugin->createEngine()};
    ASSERT_TRUE(createEngineResult.isOk());

    IEngine* const engine = createEngineResult.result.value();
    ASSERT_TRUE(engine);
    ASSERT_TRUE(engine->queryInterface<IEngine>());

    engine->setHandler(makePtr<EngineHandler>().get());

    testEngineManifest(engine);
    testEngineSettings(engine);

    testStubAnalyticsPluginEngine(engine);
}

/** Any Plugin must pass. */
static void testPlugin(Ptr<nx::sdk::IPlugin> plugin)
{
    ASSERT_TRUE(plugin);
    ASSERT_TRUE(plugin->queryInterface<IRefCountable>());
    ASSERT_TRUE(plugin->queryInterface<nx::sdk::IPlugin>());

    plugin->setUtilityProvider(makePtr<UtilityProvider>().get());

    const Ptr<nx::sdk::analytics::IPlugin> analyticsPlugin =
        plugin->queryInterface<nx::sdk::analytics::IPlugin>();

    testAnalyticsPlugin(analyticsPlugin.get());
}

//-------------------------------------------------------------------------------------------------
// Infrastructure for obtaining IPlugin instances to test.

using LibHandle =
    #if defined(_WIN32)
        HMODULE;
    #else
        void*;
    #endif

static std::string getLastDynamicLibError()
{
    #if defined(_WIN32)
        const DWORD errorCode = GetLastError();

        char16_t msgBuf[1024];
        memset(msgBuf, 0, sizeof(msgBuf));

        FormatMessageW(
            /*dwFlags*/ FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS,
            /*lpSource*/ nullptr,
            /*dwMessageId*/ errorCode,
            /*dwLanguageId*/ MAKELANGID(LANG_SYSTEM_DEFAULT, SUBLANG_SYS_DEFAULT),
            (LPWSTR) &msgBuf,
            sizeof(msgBuf),
            /*Arguments*/ nullptr);

        int msgBufLen = (int) wcslen((const wchar_t*) msgBuf);

        // Remove all trailing whitespace.
        for (int i = msgBufLen; i > 0; --i)
        {
            if (msgBuf[i - 1] <= L' ')
            {
                msgBuf[i - 1] = L'\0';
                --msgBufLen;
            }
            else
            {
                break;
            }
        }

        // Remove all leading whitespace.
        const char16_t* msgStartPos = msgBuf;
        int msgLen = msgBufLen;
        while (*msgStartPos && *msgStartPos <= L' ')
        {
            ++msgStartPos;
            --msgLen;
        }

        // Convert the message: UTF-16 -> UTF-8.
        // NOTE: int16_t is used instead of char16_t as a workaround for an MSVC2015 bug.
        std::wstring_convert<std::codecvt_utf8_utf16<int16_t>, int16_t> converter;
        const int16_t* const startPos = (const int16_t*) msgStartPos;
        const std::string msg = converter.to_bytes(startPos, startPos + msgLen);

        const std::string errorCodeMessage = nx::kit::utils::format(
            "Windows error %d (0x%X)", errorCode, errorCode);
        if (msg.empty())
            return errorCodeMessage;
        return errorCodeMessage + ": " + msg;
    #else
        return dlerror();
    #endif
}

static LibHandle loadLib(const std::string& libFilename)
{
    #if defined(_WIN32)
        // Convert the filename: UTF-8 -> UTF-16.
        // NOTE: int16_t is used instead of char16_t as a workaround for an MSVC2015 bug.
        std::wstring_convert<std::codecvt_utf8_utf16<int16_t>, int16_t> converter;
        const std::basic_string<int16_t> libFilenameW = converter.from_bytes(libFilename);

        const LibHandle libHandle = LoadLibraryW((wchar_t*) libFilenameW.c_str());
    #else
        const LibHandle libHandle = dlopen(libFilename.c_str(), /*resolve all funcs*/ RTLD_NOW);
    #endif

    if (!libHandle)
    {
        NX_PRINT << "ERROR: Unable to load the plugin library "
            << nx::kit::utils::toString(libFilename) //< Enquote and C-escape.
            << ": " << getLastDynamicLibError();
        ASSERT_TRUE(libHandle); //< Fail the test.
    }
    return libHandle;
}

static void unloadLib(LibHandle libHandle)
{
    #if defined(_WIN32)
        const BOOL freeLibrarySuccess = FreeLibrary(libHandle);
        if (!freeLibrarySuccess)
        {
            NX_PRINT << "ERROR: Unable to unload the plugin library: " << getLastDynamicLibError();
            ASSERT_TRUE(freeLibrarySuccess); //< Fail the test.
        }
    #else
        const int dlcloseResult = dlclose(libHandle);
        if (dlcloseResult != 0)
        {
            NX_PRINT << "ERROR: Unable to unload the plugin library: " << getLastDynamicLibError();
            ASSERT_EQ(0, dlcloseResult); //< Fail the test.
        }
    #endif
}

static void* resolveLibFunc(LibHandle libHandle, const std::string& funcName)
{
    #if defined(_WIN32)
        return GetProcAddress(libHandle, funcName.c_str());
    #else
        return dlsym(libHandle, funcName.c_str());
    #endif
}

static std::string getPluginLibName(const std::string& libFilename)
{
    const std::string baseName = nx::kit::utils::baseName(libFilename);

    // Remove extension.
    #if defined(_WIN32)
        static const std::string kLibSuffix = ".dll";
    #else
        static const std::string kLibSuffix = ".so";
    #endif
    ASSERT_TRUE(nx::kit::utils::stringEndsWith(baseName, kLibSuffix));
    const std::string baseNameWithoutExt = baseName.substr(0, baseName.size() - kLibSuffix.size());

    #if defined(_WIN32)
        return baseNameWithoutExt;
    #else
        // Remove Linux prefix "lib".
        static const std::string kLibPrefix = "lib";
        ASSERT_TRUE(nx::kit::utils::stringStartsWith(baseNameWithoutExt, kLibPrefix));
        return baseNameWithoutExt.substr(kLibPrefix.size());
    #endif
}

static void testPluginLibrary(const std::string& libFilename)
{
    NX_PRINT << "Testing plugin library " << nx::kit::utils::toString(libFilename);

    const auto libHandle = loadLib(libFilename);

    const auto entryPointFunc = reinterpret_cast<nx::sdk::IPlugin::EntryPointFunc>(
        resolveLibFunc(libHandle, nx::sdk::IPlugin::kEntryPointFuncName));

    const auto multiEntryPointFunc = reinterpret_cast<nx::sdk::IPlugin::MultiEntryPointFunc>(
        resolveLibFunc(libHandle, nx::sdk::IPlugin::kMultiEntryPointFuncName));

    // Obtain plugin's LibContext and fill in the plugin libName.
    const auto nxLibContextFunc = reinterpret_cast<NxLibContextFunc>(
        resolveLibFunc(libHandle, kNxLibContextFuncName));
    ASSERT_TRUE(nxLibContextFunc);
    ILibContext* const pluginLibContext = nxLibContextFunc();
    ASSERT_TRUE(pluginLibContext);
    const std::string pluginLibName = getPluginLibName(libFilename);
    pluginLibContext->setName(pluginLibName.c_str());

    ASSERT_TRUE(entryPointFunc || multiEntryPointFunc);
    if (multiEntryPointFunc) //< Do not use entryPointFunc even if it is exported.
    {
        int instanceIndex = 0;
        while (const auto plugin = Ptr(multiEntryPointFunc(instanceIndex)))
        {
            if (g_pluginInstanceIndex == -1 || g_pluginInstanceIndex == instanceIndex)
            {
                NX_PRINT << "Testing plugin " << pluginLibName
                    << " with instance index " << instanceIndex;
                testPlugin(plugin);
            }
            ++instanceIndex;
        }
    }
    else //< Use entryPointFunc.
    {
        const auto plugin = Ptr(entryPointFunc());
        ASSERT_TRUE(plugin);
        testPlugin(plugin);
    }

    unloadLib(libHandle);
}

static bool findPluginLibFilenames(const std::string& argv0)
{
    // Read filenames from the config file which resides in the executable directory and has name
    // analytics_plugin_ut.cfg; empty lines and lines starting with `#` are ignored. Relative
    // paths are treated as relative to the executable directory.

    const std::string exeDir = argv0.substr(
        0, argv0.size() - nx::kit::utils::baseName(argv0).size());
    const std::string cfgFilename = exeDir + "analytics_plugin_ut.cfg";

    std::ifstream file(cfgFilename);
    if (!file.good())
    {
        NX_PRINT << "ERROR: Unable to load test config file " << cfgFilename;
        return false;
    }

    std::string lineStr;
    int line = 0;
    while (std::getline(file, lineStr))
    {
        lineStr = nx::kit::utils::trimString(lineStr);
        if (lineStr.empty() || nx::kit::utils::stringStartsWith(lineStr, "#"))
            continue;
        g_pluginLibFilenames.emplace_back(nx::kit::utils::absolutePath(exeDir, lineStr));
        ++line;
    }

    if (g_pluginLibFilenames.empty())
    {
        NX_PRINT << "ERROR: No plugins to test found in "
            << nx::kit::utils::toString(exeDir) //< Enquote and C-escape.
            << ".";
        return false;
    }

    return true;
}

static bool collectPluginLibFilenames(
    const char* argv0, int extraArgCount, const char* extraArgs[])
{
    if (extraArgCount >= 1 && extraArgs[0])
    {
        // Test the specified plugin.

        const std::string pluginLibFilename = extraArgs[0];
        if (pluginLibFilename.empty())
        {
            std::cerr << "ERROR: The specified plugin library filename is empty.\n";
            return false;
        }

        g_pluginLibFilenames.emplace_back(pluginLibFilename);

        if (extraArgCount == 2 && extraArgs[1])
        {
            if (!nx::kit::utils::fromString(extraArgs[1], &g_pluginInstanceIndex))
            {
                NX_PRINT << "ERROR: Invalid plugin instance index.";
                return false;
            }
        }

        return true;
    }

    if (extraArgCount != 0)
    {
        NX_PRINT << "ERROR: Expected either no extra args, or a libName of a plugin to test.";
        return false;
    }

    return findPluginLibFilenames(argv0);
}

TEST(analytics_plugin, test)
{
    libContext().setName("analytics_plugin_ut");

    for (const auto& pluginLibFilename: g_pluginLibFilenames)
        testPluginLibrary(pluginLibFilename);
}

} // namespace test
} // namespace analytics
} // namespace vms_server_plugins
} // namespace nx

using namespace nx::vms_server_plugins::analytics::test;

int main(int argc, const char* argv[])
{
    // Process only args after "--" - let nx_kit test-infrastructure process the others.
    // Process only args after "--" - let nx_kit test infrastructure process the others.
    int extraArgCount = 0;
    const char** extraArgs = nullptr;
    for (int i = 1; i < argc; ++i)
    {
        if (argv[i] && std::string(argv[i]) == "--")
        {
            extraArgCount = argc - i - 1;
            extraArgs = (extraArgCount == 0) ? nullptr : &argv[i + 1];
            break;
        }
    }

    try
    {
        if (!collectPluginLibFilenames(argv[0], extraArgCount, extraArgs))
            return 1;
    }
    catch (const std::exception& e)
    {
        NX_PRINT << "\nERROR: Unexpected exception:\n    " << e.what() << std::endl;
        return 1;
    }
    catch (...)
    {
        NX_PRINT << "\nERROR: Unknown exception." << std::endl;
        return 1;
    }

    // NOTE: runnAllTests() handles all exceptions.
    return nx::kit::test::runAllTests("analytics_plugin", /*suppress newline*/ 1 + (const char*)
R"(
  <plugin-library-filename> [<instance-index>]
    Run the test for the specified plugin library filename and the specified sub-plugin (if not
    specified, all sub-plugins are tested); otherwise, take filenames from the config file
    "analytics_plugin_ut.cfg" residing next to this test executable.
)");
}
