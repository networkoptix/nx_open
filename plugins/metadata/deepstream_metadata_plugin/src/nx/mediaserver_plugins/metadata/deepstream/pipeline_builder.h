#pragma once

#include <nx/gstreamer/pipeline.h>

namespace nx {
namespace mediaserver_plugins {
namespace metadata {
namespace deepstream {

class PipelineBuilder
{

public:
    virtual ~PipelineBuilder() = default;
    virtual std::unique_ptr<nx::gstreamer::Pipeline> build(const std::string& pipelineName) = 0;
};

} // namespace deepstream
} // namespace metadata
} // namespace mediaserver_plugins
} // namespace nx
