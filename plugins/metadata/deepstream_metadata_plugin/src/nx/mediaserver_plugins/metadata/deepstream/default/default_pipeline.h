#pragma once

#include <nx/mediaserver_plugins/metadata/deepstream/plugin.h>
#include <nx/mediaserver_plugins/metadata/deepstream/base_pipeline.h>
#include <nx/mediaserver_plugins/metadata/deepstream/default/tracking_mapper.h>
#include <nx/mediaserver_plugins/metadata/deepstream/default/object_class_description.h>

namespace nx {
namespace mediaserver_plugins {
namespace metadata {
namespace deepstream {

class DefaultPipeline: public BasePipeline
{
    using base_type = BasePipeline;
public:
    DefaultPipeline(
        const nx::gstreamer::ElementName& pipelineName,
        nx::mediaserver_plugins::metadata::deepstream::Plugin* plugin);

    TrackingMapper* trackingMapper();

    const std::vector<ObjectClassDescription>& objectClassDescriptions() const;

private:
    TrackingMapper m_trackingMapper;
    std::vector<ObjectClassDescription> m_objectClassDescriptions;
    int64_t m_lastFrameTimestampUs = 0;
};

} // namespace deepstream
} // namespace metadata
} // namespace mediserver_plugins
} // namespace nx
