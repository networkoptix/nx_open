#include "manager.h"

#include <plugins/plugin_tools.h>

#include <nx/sdk/metadata/compressed_video_packet.h>
#include <nx/sdk/metadata/common_metadata_packet.h>

#include <nx/mediaserver_plugins/metadata/deepstream/deepstream_common.h>
#include <nx/mediaserver_plugins/metadata/deepstream/openalpr_common.h>
#include <nx/mediaserver_plugins/metadata/deepstream/deepstream_metadata_plugin_ini.h>
#include <nx/mediaserver_plugins/metadata/deepstream/default/default_pipeline_builder.h>
#include <nx/mediaserver_plugins/metadata/deepstream/openalpr/openalpr_pipeline_builder.h>

#define NX_PRINT_PREFIX "metadata::deepstream::Manager::"
#include <nx/kit/debug.h>

namespace nx {
namespace mediaserver_plugins {
namespace metadata {
namespace deepstream {

using namespace nx::sdk;
using namespace nx::sdk::metadata;

Manager::Manager(
    nx::mediaserver_plugins::metadata::deepstream::Plugin* plugin,
    const std::string& id)
    :
    m_plugin(plugin)

{
    NX_OUTPUT << __func__ << "(\"" << plugin->name() << "\") -> " << this;

    std::unique_ptr<BasePipelineBuilder> builder;

    if (ini().pipelineType == kOpenAlprPipeline)
        builder = std::make_unique<OpenAlprPipelineBuilder>(plugin);
    else
        builder = std::make_unique<DefaultPipelineBuilder>(plugin);

    m_pipeline = builder->build(id);
    m_pipeline->start();
}

void* Manager::queryInterface(const nxpl::NX_GUID& interfaceId)
{
    if (interfaceId == nx::sdk::metadata::IID_CameraManager)
    {
        addRef();
        return static_cast<nx::sdk::metadata::CameraManager*>(this);
    }

    if (interfaceId == nx::sdk::metadata::IID_ConsumingCameraManager)
    {
        addRef();
        return static_cast<nx::sdk::metadata::ConsumingCameraManager*>(this);
    }

    if (interfaceId == nxpl::IID_PluginInterface)
    {
        addRef();
        return static_cast<nxpl::PluginInterface*>(this);
    }
    return nullptr;
}

void Manager::setDeclaredSettings(const nxpl::Setting *settings, int count)
{
    NX_OUTPUT << __func__ << " Received Manager settings:";
    NX_OUTPUT << "{";
    for (int i = 0; i < count; ++i)
    {
        NX_OUTPUT << "    \"" << settings[i].name
            << "\": \"" << settings[i].value << "\""
            << ((i < count - 1) ? "," : "");
    }
    NX_OUTPUT << "}";

}

Error Manager::setHandler(MetadataHandler* handler)
{
    NX_OUTPUT << __func__ << " Setting metadata handler";
    std::lock_guard<std::mutex> lock(m_mutex);
    m_handler = handler;
    m_pipeline->setMetadataCallback(
        [this](MetadataPacket* packet)
        {
            NX_OUTPUT << __func__ << " Calling metadata handler";
            m_handler->handleMetadata(Error::noError, packet);
        });

    NX_OUTPUT << __func__ << "() END -> noError";
    return Error::noError;
}

Error Manager::startFetchingMetadata(
    const char* const* /*typeList*/, int /*typeListSize*/)
{
    NX_OUTPUT << __func__ << " Starting to fetch metadata. Doing nothing, actually...";
    return Error::noError;
}

nx::sdk::Error Manager::pushDataPacket(nx::sdk::metadata::DataPacket* dataPacket)
{
#if 0
    nxpt::ScopedRef<nx::sdk::metadata::CompressedVideoPacket> compressedVideo(
        dataPacket->queryInterface(nx::sdk::metadata::IID_CompressedVideoPacket));

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

Error Manager::stopFetchingMetadata()
{
    NX_OUTPUT << __func__ << " Stopping to fetch metadata. Doing nothing, actually...";
    return Error::noError;
}

const char* Manager::capabilitiesManifest(Error* error)
{
    *error = Error::noError;

    static const std::string kManifestPrefix = (R"json(
        {
            "supportedObjectTypes": [)json");

    static const std::string kManifestPostfix = "]}";

    if (!m_manifest.empty())
        return m_manifest.c_str();

    m_manifest = kManifestPrefix;
    const auto& objectClassDescritions = m_plugin->objectClassDescritions();

    if (ini().pipelineType == kOpenAlprPipeline)
    {
        m_manifest += "\"" + kLicensePlateGuid +"\"";
    }
    else
    {
        for (auto i = 0; i < objectClassDescritions.size(); ++i)
        {
            m_manifest += "\"" + objectClassDescritions[i].typeId + "\"";

            if (i < objectClassDescritions.size() - 1)
                m_manifest += ',';
        }
    }

    m_manifest += kManifestPostfix;
    return m_manifest.c_str();
}

void Manager::freeManifest(const char* data)
{
    NX_OUTPUT << __func__ << " Freeing manifest, doing nothing actually...";
}

Manager::~Manager()
{
    NX_OUTPUT << __func__ << "(" << this << ") BEGIN";
    stopFetchingMetadata();
    NX_OUTPUT << __func__ << "(" << this << ") END";
}

} // namespace deepstream
} // namespace metadata
} // namespace mediaserver_plugins
} // namespace nx
