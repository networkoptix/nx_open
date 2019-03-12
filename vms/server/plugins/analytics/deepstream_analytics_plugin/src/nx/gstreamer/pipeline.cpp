#include "pipeline.h"

namespace nx {
namespace gstreamer {

Pipeline::Pipeline(const ElementName& pipelineName)
{
    m_element = NativeElementPtr(
        GST_ELEMENT(gst_pipeline_new(pipelineName.c_str())),
                [](GstElement* element) { gst_object_unref(element); });
}

Pipeline::~Pipeline()
{
}

Pipeline::Pipeline(Pipeline&& other):
    base_type(std::move(other))
{
}

void Pipeline::setBusCallback(BusCallback /*busCallback*/)
{
    // Do nothing.
}

} // namespace gstreamer
} // namespace nx
