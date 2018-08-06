#include <iostream>
#include <vector>

#include <nx/kit/debug.h>
#include <nx/kit/test.h>

#include <plugins/plugin_api.h>
#include <plugins/plugin_tools.h>

#include <nx/sdk/metadata/plugin.h>
#include <nx/sdk/metadata/consuming_camera_manager.h>
#include <nx/sdk/metadata/uncompressed_video_frame.h>
#include <nx/sdk/metadata/compressed_video_packet.h>

extern "C" {

nxpl::PluginInterface* createNxMetadataPlugin();

} // extern "C"

static const int noError = (int) nx::sdk::Error::noError;

static void testPluginManifest(nx::sdk::metadata::Plugin* plugin)
{
    nx::sdk::Error error = nx::sdk::Error::noError;
    const char* manifest = plugin->capabilitiesManifest(&error);
    ASSERT_TRUE(manifest != nullptr);
    ASSERT_EQ((int) nx::sdk::Error::noError, (int) error);
    std::string manifestStr = manifest;
    ASSERT_TRUE(!manifestStr.empty());
    NX_PRINT << "Plugin manifest:\n" << manifest;

    // This test assumes that the plugin consumes compressed frames - verify it in the manifest.
    ASSERT_EQ(std::string::npos, manifestStr.find("needUncompressedVideoFrames"));
}

static void testCameraManagerManifest(nx::sdk::metadata::CameraManager* cameraManager)
{
    nx::sdk::Error error = nx::sdk::Error::noError;
    const char* manifest = cameraManager->capabilitiesManifest(&error);
    ASSERT_TRUE(manifest != nullptr);
    ASSERT_EQ(noError, (int) error);
    std::string manifestStr = manifest;
    cameraManager->freeManifest(manifest);
    ASSERT_TRUE(!manifestStr.empty());
    NX_PRINT << "CameraManager manifest:\n" << manifest;
}

static void testPluginSettings(nx::sdk::metadata::Plugin* plugin)
{
    const nxpl::Setting settings[] = {
        {"setting1", "value1"},
        {"setting2", "value2"},
    };

    plugin->setDeclaredSettings(nullptr, 0); //< Test assigning empty settings.
    plugin->setDeclaredSettings(settings, 1); //< Test assigning a single setting.
    plugin->setDeclaredSettings(settings, sizeof(settings) / sizeof(settings[0]));
}

static void testCameraManagerSettings(nx::sdk::metadata::CameraManager* cameraManager)
{
    const nxpl::Setting settings[] = {
        {"setting1", "value1"},
        {"setting2", "value2"},
    };

    cameraManager->setDeclaredSettings(nullptr, 0); //< Test assigning empty settings.
    cameraManager->setDeclaredSettings(settings, 1); //< Test assigning a single setting.
    cameraManager->setDeclaredSettings(settings, sizeof(settings) / sizeof(settings[0]));
}

class Action: public nx::sdk::metadata::Action
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
    virtual nxpl::NX_GUID cameraId() override { return m_cameraId; }
    virtual int64_t timestampUs() override { return m_timestampUs; }

    virtual const nxpl::Setting* params() override
    {
        return paramCount() == 0 ? nullptr : &m_params.front();
    }

    virtual int paramCount() override { return m_params.size(); }

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
    nxpl::NX_GUID m_objectId = {0};
    nxpl::NX_GUID m_cameraId = {0};
    int64_t m_timestampUs = 0;
    bool m_handleResultCalled = false;
    bool m_expectedNonNullActionUrl = false;
    bool m_expectedNonNullMessageToUser = false;

private:
    std::vector<std::pair<std::string, std::string>> m_paramsData; //< Owns strings for m_params.
    std::vector<nxpl::Setting> m_params; //< Points to strings from m_paramsData.
};

static void testExecuteActionNonExisting(nx::sdk::metadata::Plugin* plugin)
{
    Action action;
    action.m_actionId = "non_existing_actionId";

    nx::sdk::Error error = nx::sdk::Error::noError;
    plugin->executeAction(&action, &error);
    ASSERT_TRUE(error != nx::sdk::Error::noError);
    action.assertExpectedState();
}

static void testExecuteActionAddToList(nx::sdk::metadata::Plugin* plugin)
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

static void testExecuteActionAddPerson(nx::sdk::metadata::Plugin* plugin)
{
    Action action;
    action.m_actionId = "nx.stub.addPerson";
    action.m_objectId = {0};
    action.m_expectedNonNullActionUrl = true;

    nx::sdk::Error error = nx::sdk::Error::noError;
    plugin->executeAction(&action, &error);
    ASSERT_EQ(noError, (int) error);
    action.assertExpectedState();
}

class MetadataHandler: public nx::sdk::metadata::MetadataHandler
{
public:
    void handleMetadata(nx::sdk::Error error, nx::sdk::metadata::MetadataPacket* metadata) override
    {
        ASSERT_EQ(noError, (int) error);
        ASSERT_TRUE(metadata != nullptr);

        NX_PRINT << "MetadataHandler: Received metadata packet with timestamp "
            << metadata->timestampUsec() << " us, duration " << metadata->durationUsec() << " us";

        ASSERT_TRUE(metadata->durationUsec() >= 0);
        ASSERT_TRUE(metadata->timestampUsec() >= 0);
    }
};

class CompressedVideoFrame: public nx::sdk::metadata::DataPacket
{
public:
    virtual void* queryInterface(const nxpl::NX_GUID& interfaceId)
    {
        if (interfaceId == nx::sdk::metadata::IID_CompressedVideoPacket)
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

TEST(stub_metadata_plugin, test)
{
    nxpl::PluginInterface* const plugin = createNxMetadataPlugin();

    // TODO: #mshevchenko: Use ScopedRef for all queryInterface() calls.

    ASSERT_TRUE(plugin->queryInterface(nxpl::IID_Plugin3) != nullptr);
    plugin->releaseRef();

    const auto metadataPlugin = static_cast<nx::sdk::metadata::Plugin*>(
        plugin->queryInterface(nx::sdk::metadata::IID_Plugin));
    ASSERT_TRUE(metadataPlugin != nullptr);

    std::string pluginName = metadataPlugin->name();
    ASSERT_TRUE(!pluginName.empty());
    NX_PRINT << "Plugin name: [" << pluginName << "]";

    nx::sdk::Error error = nx::sdk::Error::noError;


    testPluginManifest(metadataPlugin);
    testPluginSettings(metadataPlugin);
    testExecuteActionNonExisting(metadataPlugin);
    testExecuteActionAddToList(metadataPlugin);
    testExecuteActionAddPerson(metadataPlugin);

    nx::sdk::CameraInfo cameraInfo;
    error = nx::sdk::Error::noError;
    auto baseCameraManager = metadataPlugin->obtainCameraManager(&cameraInfo, &error);
    ASSERT_EQ(noError, (int) error);
    ASSERT_TRUE(baseCameraManager != nullptr);
    ASSERT_TRUE(
        baseCameraManager->queryInterface(nx::sdk::metadata::IID_CameraManager) != nullptr);
    auto cameraManager = static_cast<nx::sdk::metadata::ConsumingCameraManager*>(
        baseCameraManager->queryInterface(nx::sdk::metadata::IID_ConsumingCameraManager));
    ASSERT_TRUE(cameraManager != nullptr);

    testCameraManagerManifest(cameraManager);
    testCameraManagerSettings(cameraManager);

    MetadataHandler metadataHandler;
    ASSERT_EQ(noError, (int) cameraManager->setHandler(&metadataHandler));

    ASSERT_EQ(noError, (int) cameraManager->startFetchingMetadata(
        /*typeList*/ nullptr, /*typeListSize*/ 0));

    CompressedVideoFrame compressedVideoFrame;
    ASSERT_EQ(noError, (int) cameraManager->pushDataPacket(&compressedVideoFrame));

    ASSERT_EQ(noError, (int) cameraManager->stopFetchingMetadata());

    cameraManager->releaseRef();

    plugin->releaseRef();
}

int main()
{
    return nx::kit::test::runAllTests();
}
