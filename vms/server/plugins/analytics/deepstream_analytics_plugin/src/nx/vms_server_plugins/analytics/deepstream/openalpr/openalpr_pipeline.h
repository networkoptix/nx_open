#pragma once

#include <nx/vms_server_plugins/analytics/deepstream/base_pipeline.h>
#include <nx/vms_server_plugins/analytics/deepstream/openalpr/simple_license_plate_tracker.h>

namespace nx {
namespace vms_server_plugins {
namespace analytics {
namespace deepstream {

class OpenAlprPipeline: public BasePipeline
{
    using base_type = BasePipeline;

public:
    OpenAlprPipeline(
        const nx::gstreamer::ElementName& pipelineName,
        nx::vms_server_plugins::analytics::deepstream::Engine* engine);

    SimpleLicensePlateTracker* licensePlateTracker();

    bool shouldDropFrame(int64_t frameTimestamp) const;

private:
    SimpleLicensePlateTracker m_licensePlateTracker;
};

} // namespace deepstream
} // namespace analytics
} // namespace vms_server_plugins
} // namespace nx
