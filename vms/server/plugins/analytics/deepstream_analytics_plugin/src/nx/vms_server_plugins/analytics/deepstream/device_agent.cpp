#include "device_agent.h"

#include <nx/sdk/helpers/ptr.h>
#include <nx/sdk/helpers/string.h>

#include <nx/sdk/analytics/i_compressed_video_packet.h>
#include <nx/sdk/analytics/i_metadata_packet.h>

#include <nx/vms_server_plugins/analytics/deepstream/deepstream_common.h>
#include <nx/vms_server_plugins/analytics/deepstream/openalpr_common.h>
#include <nx/vms_server_plugins/analytics/deepstream/deepstream_analytics_plugin_ini.h>
#include <nx/vms_server_plugins/analytics/deepstream/default/default_pipeline_builder.h>
#include <nx/vms_server_plugins/analytics/deepstream/openalpr/openalpr_pipeline_builder.h>

#define NX_PRINT_PREFIX "deepstream::DeviceAgent::"
#include <nx/kit/debug.h>

namespace nx {
namespace vms_server_plugins {
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

void DeviceAgent::setSettings(const IStringMap* settings)
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

IStringMap* DeviceAgent::pluginSideSettings() const
{
    return nullptr;
}

Error DeviceAgent::setHandler(IDeviceAgent::IHandler* handler)
{
    NX_OUTPUT << __func__ << " Setting metadata handler";
    std::lock_guard<std::mutex> lock(m_mutex);

    if (!handler)
    {
        m_handler.reset();
        return Error::noError;
    }

    handler->addRef();
    m_handler.reset(handler);
    
    m_pipeline->setMetadataCallback(
        [this](IMetadataPacket* packet)
        {
            NX_OUTPUT << __func__ << " Calling metadata handler";
            m_handler->handleMetadata(packet);
        });

    NX_OUTPUT << __func__ << "() END -> noError";
    return Error::noError;
}

Error DeviceAgent::pushDataPacket(IDataPacket* dataPacket)
{
// TODO: Investigate why this code is commented out.
#if 0
    if (!queryInterfacePtr<ICompressedVideoPacket>(dataPacket))
        return Error::noError;

    NX_OUTPUT << __func__ << " Frame timestamp is: " << dataPacket->timestampUs();
#endif

    NX_OUTPUT
        << __func__
        << " Pushing data packet to pipeline";

    m_pipeline->pushDataPacket(dataPacket);
    return Error::noError;
}

Error DeviceAgent::setNeededMetadataTypes(const IMetadataTypes* metadataTypes)
{
    if (metadataTypes->isEmpty())
    {
        stopFetchingMetadata();
        return Error::noError;
    }

    return startFetchingMetadata(metadataTypes);
}

Error DeviceAgent::startFetchingMetadata(const IMetadataTypes* /*metadataTypes*/)
{
    NX_OUTPUT << __func__ << " Starting to fetch metadata. Doing nothing, actually...";
    return Error::noError;
}

void DeviceAgent::stopFetchingMetadata()
{
    NX_OUTPUT << __func__ << " Stopping to fetch metadata. Doing nothing, actually...";
}

const IString* DeviceAgent::manifest(Error* error) const
{
    *error = Error::noError;

    if (!m_manifest.empty())
        return new nx::sdk::String(m_manifest);

    std::string objectTypeIds;
    const auto& objectClassDescritions = m_engine->objectClassDescritions();
    static const std::string kIndent = "        ";
    if (ini().pipelineType == kOpenAlprPipeline)
    {
        objectTypeIds += kIndent + "\"" + kLicensePlateObjectTypeId +"\"";
    }
    else
    {
        for (std::size_t i = 0; i < objectClassDescritions.size(); ++i)
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
    ],
    "supportedEventTypeIds": [
        ")json" + kLicensePlateDetectedEventTypeId + R"json("
    ]
}
)json";

    return new nx::sdk::String(m_manifest);
}

DeviceAgent::~DeviceAgent()
{
    NX_OUTPUT << __func__ << "(" << this << ") BEGIN";
    stopFetchingMetadata();
    NX_OUTPUT << __func__ << "(" << this << ") END";
}

} // namespace deepstream
} // namespace analytics
} // namespace vms_server_plugins
} // namespace nx
