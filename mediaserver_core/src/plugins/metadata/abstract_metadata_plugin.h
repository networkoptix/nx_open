#pragma once

#include <plugins/plugin_api.h>
#include <plugins/metadata/abstract_metadata_manager.h>
#include <plugins/metadata/abstract_serializator.h>
#include <plugins/metadata/utils.h>

namespace nx {
namespace sdk {
namespace metadata {

static const nxpl::GUID IID_CompressedAudio
    = {{0x6d, 0x73, 0x71, 0x36, 0x17, 0xad, 0x43, 0xf9, 0x9f, 0x80, 0x7d, 0x56, 0x91, 0x36, 0x82, 0x94}};

class AbstractMetadataPlugin: public nxpl::PluginInterface
{
    virtual AbstractMetadataManager* managerForResource(const ResourceInfo& resourceInfo, Error* error) = 0;

    virtual AbstractSerializator* serializatorForType(const nxpl::GUID& typeGuid, Error* error) = 0;
};

} // namespace metadata
} // namespace sdk
} // namespace nx

extern "C" {

} // extern "C"
