#pragma once

#include "abstract_metadata_plugin.h"

#include <core/resource/resource.h>
#include <common/common_module_aware.h>

namespace nx {
namespace analytics {

class MetadataPluginFactory: public QnCommonModuleAware
{
public:
    MetadataPluginFactory(QnCommonModule* commonModule);

    std::unique_ptr<AbstractMetadataPlugin> createMetadataPlugin(
        const QnResourcePtr& resource,
        MetadataType metdataType);

private:
    QString makeId(const QnResourcePtr& resource, MetadataType metadataType) const;
};

} // namespace analytics
} // namespace nx
