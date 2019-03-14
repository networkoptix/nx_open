#pragma once

#include <vector>

#include <nx/vms_server_plugins/analytics/deepstream/engine.h>
#include <nx/vms_server_plugins/analytics/deepstream/base_pipeline.h>
#include <nx/vms_server_plugins/analytics/deepstream/default/tracking_mapper.h>
#include <nx/vms_server_plugins/analytics/deepstream/default/object_class_description.h>

namespace nx {
namespace vms_server_plugins {
namespace analytics {
namespace deepstream {

class DefaultPipeline: public BasePipeline
{
    using base_type = BasePipeline;
    
public:
    DefaultPipeline(
        const nx::gstreamer::ElementName& pipelineName,
        nx::vms_server_plugins::analytics::deepstream::Engine* engine);

    TrackingMapper* trackingMapper();

    const std::vector<ObjectClassDescription>& objectClassDescriptions() const;

private:
    TrackingMapper m_trackingMapper;
    std::vector<ObjectClassDescription> m_objectClassDescriptions;
    int64_t m_lastFrameTimestampUs = 0;
};

} // namespace deepstream
} // namespace analytics
} // namespace mediserver_plugins
} // namespace nx
