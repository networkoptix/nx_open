#pragma once

#include <memory>
#include <functional>
#include <map>

extern "C" {

#include <gst/gst.h>

} // extern "C"

namespace nx {
namespace gstreamer {

using NativePadPtr = std::unique_ptr<GstPad, std::function<void(GstPad*)>>;
using PadName = std::string;
using PadProbeId = gulong;
using ProbeFunc = GstPadProbeReturn(*)(GstPad*, GstPadProbeInfo*, gpointer userInfo);

class Pad
{
public:
    Pad(GstPad* pad);
    PadProbeId addProbe(ProbeFunc func, GstPadProbeType probeType, gpointer userData);
    void removeProbe(PadProbeId probeId);

    GstPad* nativePad() const;

private:
    NativePadPtr m_nativePad;
};

} // namespace gstreamer
} // namespace nx
