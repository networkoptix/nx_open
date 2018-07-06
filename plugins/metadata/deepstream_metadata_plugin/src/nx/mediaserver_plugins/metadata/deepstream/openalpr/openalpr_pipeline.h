#pragma once

#include <nx/mediaserver_plugins/metadata/deepstream/base_pipeline.h>
#include <nx/mediaserver_plugins/metadata/deepstream/openalpr/simple_license_plate_tracker.h>

namespace nx {
namespace mediaserver_plugins {
namespace metadata {
namespace deepstream {

class OpenAlprPipeline: public BasePipeline
{
    using base_type = BasePipeline;

public:
    OpenAlprPipeline(
        const nx::gstreamer::ElementName& pipelineName,
        nx::mediaserver_plugins::metadata::deepstream::Plugin* plugin);

    SimpleLicensePlateTracker* licensePlateTracker();

    bool shouldDropFrame(int64_t frameTimestamp) const;

private:
    SimpleLicensePlateTracker m_licensePlateTracker;
};

} // namespace deepstream
} // namespace metadata
} // namespace mediaserver_plugins
} // namespace nx
