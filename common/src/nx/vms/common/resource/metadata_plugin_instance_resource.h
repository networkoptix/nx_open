#pragma once

#include <core/resource/resource.h>

namespace nx::vms::common {

class MetadataPluginInstanceResource: public QnResource
{
    using base_type = QnResource;

public:
    MetadataPluginInstanceResource(QnCommonModule* commonModule = nullptr);
    virtual ~MetadataPluginInstanceResource() override;

private:
};

} // namespace nx::vms::common
