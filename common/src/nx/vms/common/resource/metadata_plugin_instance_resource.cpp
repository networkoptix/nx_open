#include "metadata_plugin_instance_resource.h"

namespace nx::vms::common {

MetadataPluginInstanceResource::MetadataPluginInstanceResource(QnCommonModule* commonModule):
    base_type(commonModule)
{
}

MetadataPluginInstanceResource::~MetadataPluginInstanceResource() = default;

} // namespace nx::vms::common
