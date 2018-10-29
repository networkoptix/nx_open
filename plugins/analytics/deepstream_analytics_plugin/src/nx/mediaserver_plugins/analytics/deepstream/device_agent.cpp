#include "device_agent.h"

#include <plugins/plugin_tools.h>

#include <nx/sdk/analytics/compressed_video_packet.h>
#include <nx/sdk/analytics/common_metadata_packet.h>

#include <nx/mediaserver_plugins/analytics/deepstream/deepstream_common.h>
#include <nx/mediaserver_plugins/analytics/deepstream/openalpr_common.h>
#include <nx/mediaserver_plugins/analytics/deepstream/deepstream_analytics_plugin_ini.h>
#include <nx/mediaserver_plugins/analytics/deepstream/default/default_pipeline_builder.h>
#include <nx/mediaserver_plugins/analytics/deepstream/openalpr/openalpr_pipeline_builder.h>

#define NX_PRINT_PREFIX "deepstream::DeviceAgent::"
#include <nx/kit/debug.h>

namespace nx {
namespace mediaserver_plugins {
namespace analytics {
namespace deepstream {

using namespace nx::sdk;
using namespace nx::sdk::analytics;

DeviceAgent::DeviceAgent(Engine* engine, const std::string& id): m_engine(engine)
{
    NX_OUTPUT << __func__ << "(\"" << m_engine->plugin()->name() << "\") -> " << this;

    std::unique_ptr<BasePipelineBuilder> builder;

    if (ini().pipelineType == kOpenAlprPipeline)
        builder = std::make_unique<OpenAlprPipelineBuilder>(m_engine);
    else
        builder = std::make_unique<DefaultPipelineBuilder>(m_engine);

    m_pipeline = builder->build(id);
    m_pipeline->start();
}

void* DeviceAgent::queryInterface(const nxpl::NX_GUID& interfaceId)
{
    if (interfaceId == nx::sdk::analytics::IID_DeviceAgent)
    {
        addRef();
        return static_cast<nx::sdk::analytics::DeviceAgent*>(this);
    }

    if (interfaceId == nx::sdk::analytics::IID_ConsumingDeviceAgent)
    {
        addRef();
        return static_cast<nx::sdk::analytics::ConsumingDeviceAgent*>(this);
    }

    if (interfaceId == nxpl::IID_PluginInterface)
    {
        addRef();
        return static_cast<nxpl::PluginInterface*>(this);
    }
    return nullptr;
}

void DeviceAgent::setSettings(const nx::sdk::Settings* settings)
{
    NX_OUTPUT << __func__ << " Received  settings:";
    NX_OUTPUT << "{";

    const auto count = settings->count();
    for (int i = 0; i < count; ++i)
    {
        NX_OUTPUT << "    " << settings->key(i)
            << ": " << settings->value(i)
            << ((i < count - 1) ? "," : "");
    }
    NX_OUTPUT << "}";
}

nx::sdk::Settings* DeviceAgent::settings() const
{
    return nullptr;
}

Error DeviceAgent::setMetadataHandler(MetadataHandler* metadataHandler)
{
    NX_OUTPUT << __func__ << " Setting metadata handler";
    std::lock_guard<std::mutex> lock(m_mutex);
    m_metadataHandler = metadataHandler;
    m_pipeline->setMetadataCallback(
        [this](MetadataPacket* packet)
        {
            NX_OUTPUT << __func__ << " Calling metadata handler";
            m_metadataHandler->handleMetadata(Error::noError, packet);
        });

    NX_OUTPUT << __func__ << "() END -> noError";
    return Error::noError;
}

Error DeviceAgent::startFetchingMetadata(
    const char* const* /*typeList*/, int /*typeListSize*/)
{
    NX_OUTPUT << __func__ << " Starting to fetch metadata. Doing nothing, actually...";
    return Error::noError;
}

nx::sdk::Error DeviceAgent::pushDataPacket(nx::sdk::analytics::DataPacket* dataPacket)
{
#if 0
    nxpt::ScopedRef<nx::sdk::analytics::CompressedVideoPacket> compressedVideo(
        dataPacket->queryInterface(nx::sdk::analytics::IID_CompressedVideoPacket));

    if (!compressedVideo)
        return nx::sdk::Error::noError;

    NX_OUTPUT << __func__ << " Frame timestamp is: " << dataPacket->timestampUsec();
#endif

    NX_OUTPUT
        << __func__
        << " Pushing data packet to pipeline";

    m_pipeline->pushDataPacket(dataPacket);
    return Error::noError;
}

Error DeviceAgent::stopFetchingMetadata()
{
    NX_OUTPUT << __func__ << " Stopping to fetch metadata. Doing nothing, actually...";
    return Error::noError;
}

const char* DeviceAgent::manifest(Error* error)
{
    *error = Error::noError;

    if (!m_manifest.empty())
        return m_manifest.c_str();

    std::string objectTypeIds;
    const auto& objectClassDescritions = m_engine->objectClassDescritions();
    static const std::string kIndent = "        ";
    if (ini().pipelineType == kOpenAlprPipeline)
    {
        objectTypeIds += kIndent + "\"" + kLicensePlateGuid +"\"";
    }
    else
    {
        for (auto i = 0; i < objectClassDescritions.size(); ++i)
        {
            objectTypeIds += kIndent + "\"" + objectClassDescritions[i].typeId + "\"";

            if (i < objectClassDescritions.size() - 1)
                objectTypeIds += ",\n";
        }
    }

    m_manifest = /*suppress newline*/1 + R"json(
{
    "supportedObjectTypeIds": [
)json" + objectTypeIds + R"json(
    ]
}
)json";

    return m_manifest.c_str();
}

void DeviceAgent::freeManifest(const char* data)
{
    NX_OUTPUT << __func__ << " Freeing manifest, doing nothing actually...";
}

DeviceAgent::~DeviceAgent()
{
    NX_OUTPUT << __func__ << "(" << this << ") BEGIN";
    stopFetchingMetadata();
    NX_OUTPUT << __func__ << "(" << this << ") END";
}

} // namespace deepstream
} // namespace analytics
} // namespace mediaserver_plugins
} // namespace nx
