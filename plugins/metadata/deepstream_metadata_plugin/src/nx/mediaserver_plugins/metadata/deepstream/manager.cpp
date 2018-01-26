#include "manager.h"

#include <iostream>
#include <chrono>
#include <math.h>

#include <plugins/plugin_tools.h>
#include <nx/sdk/metadata/common_metadata_packet.h>
#include <nx/sdk/metadata/common_detected_event.h>
#include <nx/sdk/metadata/common_detected_object.h>
#include <nx/sdk/metadata/common_compressed_video_packet.h>

#define NX_DEBUG_ENABLE_OUTPUT true
#define NX_PRINT_PREFIX "metadata::stub::Manager::"
#include <nx/kit/debug.h>

namespace nx {
namespace mediaserver_plugins {
namespace metadata {
namespace deepstream {

namespace {

static const nxpl::NX_GUID kLineCrossingEventGuid =
    {{0x7E, 0x94, 0xCE, 0x15, 0x3B, 0x69, 0x47, 0x19, 0x8D, 0xFD, 0xAC, 0x1B, 0x76, 0xE5, 0xD8, 0xF4}};

static const nxpl::NX_GUID kObjectInTheAreaEventGuid =
    {{0xB0, 0xE6, 0x40, 0x44, 0xFF, 0xA3, 0x4B, 0x7F, 0x80, 0x7A, 0x06, 0x0C, 0x1F, 0xE5, 0xA0, 0x4C}};

static const nxpl::NX_GUID kCarDetectedEventGuid =
    {{ 0x15, 0x3D, 0xD8, 0x79, 0x1C, 0xD2, 0x46, 0xB7, 0xAD, 0xD6, 0x7C, 0x6B, 0x48, 0xEA, 0xC1, 0xFC }};

} // namespace

using namespace nx::sdk;
using namespace nx::sdk::metadata;

Manager::Manager(Plugin* plugin):
    m_plugin(plugin)
{
    NX_PRINT << __func__ << "(\"" << plugin->name() << "\") -> " << this;
}

void* Manager::queryInterface(const nxpl::NX_GUID& interfaceId)
{
    if (interfaceId == IID_MetadataManager)
    {
        addRef();
        return static_cast<AbstractMetadataManager*>(this);
    }

    if (interfaceId == IID_ConsumingMetadataManager)
    {
        addRef();
        return static_cast<AbstractConsumingMetadataManager*>(this);
    }

    if (interfaceId == nxpl::IID_PluginInterface)
    {
        addRef();
        return static_cast<nxpl::PluginInterface*>(this);
    }
    return nullptr;
}

Error Manager::setHandler(AbstractMetadataHandler* handler)
{
    NX_OUTPUT << __func__ << "() BEGIN";
    std::lock_guard<std::mutex> lock(m_mutex);
    m_handler = handler;
    NX_OUTPUT << __func__ << "() END -> noError";
    return Error::noError;
}

Error Manager::startFetchingMetadata(
    nxpl::NX_GUID* /*eventTypeList*/,
    int /*eventTypeListSize*/)
{
    NX_OUTPUT << __func__ << "() BEGIN";
    NX_OUTPUT << __func__ << "() END -> noError";
    return Error::noError;
}

nx::sdk::Error Manager::putData(nx::sdk::metadata::AbstractDataPacket* dataPacket)
{
    NX_OUTPUT << __func__ << "() BEGIN";
    NX_OUTPUT << __func__ << "() END -> noError";
    return Error::noError;
}

Error Manager::stopFetchingMetadata()
{
    NX_OUTPUT << __func__ << "() BEGIN";
    return Error::noError;
}

const char* Manager::capabilitiesManifest(Error* error)
{
    *error = Error::noError;

    // TODO: Reuse GUID constants declared in this file.
    return R"json(
        {
            "supportedEventTypes": [
                "{7E94CE15-3B69-4719-8DFD-AC1B76E5D8F4}",
                "{B0E64044-FFA3-4B7F-807A-060C1FE5A04C}"
            ],
            "supportedObjectTypes": [
                "{153DD879-1CD2-46B7-ADD6-7C6B48EAC1FC}"
            ]
        }
    )json";
}
void Manager::freeManifest(const char* data)
{
    NX_PRINT << __func__ << "Freeing manifest";
}

Manager::~Manager()
{
    NX_PRINT << __func__ << "(" << this << ") BEGIN";
    stopFetchingMetadata();
    NX_PRINT << __func__ << "(" << this << ") END";
}

} // namespace deepstream
} // namespace metadata
} // namespace mediaserver_plugins
} // namespace nx
