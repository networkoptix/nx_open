#pragma once

#include <nx/gstreamer/pipeline.h>

namespace nx {
namespace vms_server_plugins {
namespace analytics {
namespace deepstream {

class PipelineBuilder
{

public:
    virtual ~PipelineBuilder() = default;
    virtual std::unique_ptr<nx::gstreamer::Pipeline> build(const std::string& pipelineName) = 0;
};

} // namespace deepstream
} // namespace analytics
} // namespace vms_server_plugins
} // namespace nx
