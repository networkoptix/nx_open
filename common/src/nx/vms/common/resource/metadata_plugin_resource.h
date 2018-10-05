#pragma once

#include <core/resource/resource.h>

namespace nx::vms::common {

class MetadataPluginResource: public QnResource
{
    using base_type = QnResource;

public:
    MetadataPluginResource(QnCommonModule* commonModule = nullptr);
    virtual ~MetadataPluginResource() override;

private:
};

} // namespace nx::vms::common
