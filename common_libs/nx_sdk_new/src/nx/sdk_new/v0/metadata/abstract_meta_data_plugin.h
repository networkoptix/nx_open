#pragma once

#include <nx/sdk_new/v0/common/abstract_object.h>
#include <nx/sdk_new/v0/metadata/abstract_meta_data_manager.h>

namespace nx {
namespace sdk {
namespace v0 {
namespace metadata {

class AbstractMetaDataPlugin: public AbstractObject
{
    AbstractMetaDataManager* createMetadataManager(const ResourceInfo& resourceInfo) = 0;
};

} // namespace metadata
} // namespace v0
} // namespace sdk
} // namespace nx

extern "C" {

nx::sdk::v0::metadata::AbstractMetaDataPlugin* createMetadataPlugin();

} // extrern "C"
