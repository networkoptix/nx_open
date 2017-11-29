#include "metadata_plugin_factory.h"

#include <nx/utils/std/cpp14.h>
#include <common/common_module.h>

#include <analytics/plugins/detection/detection_plugin_factory.h>
#include <analytics/plugins/detection/detection_plugin_wrapper.h>
#include <analytics/plugins/detection/config.h>

namespace nx {
namespace analytics {

MetadataPluginFactory::MetadataPluginFactory(QObject* parent):
    QObject(parent),
    QnCommonModuleAware(parent)
{
}

std::unique_ptr<AbstractMetadataPlugin> MetadataPluginFactory::createMetadataPlugin(
    const QnResourcePtr& resource,
    MetadataType metadataType)
{
    switch (metadataType)
    {
        case MetadataType::ObjectDetection:
        {
            auto result = std::make_unique<DetectionPluginWrapper>();
            if (!result)
                return nullptr;

            auto detectionPluginFactory = commonModule()->detectionPluginFactory();
            if (!detectionPluginFactory)
                return nullptr;

            result->setDetector(
                detectionPluginFactory->createDetectionPlugin(makeId(resource, metadataType)));

            return std::move(result);
        }
        default:
        {
            return nullptr;
        }
    }
}

QString MetadataPluginFactory::makeId(const QnResourcePtr& resource, MetadataType metadataType) const
{
    return QString::number((int)metadataType)
        + lit("_")
        + resource->getName()
        + lit("_")
        + resource->getId().toString();
}

} // namespace analytics
} // namespace nx
