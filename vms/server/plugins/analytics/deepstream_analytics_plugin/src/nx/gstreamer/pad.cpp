#include "pad.h"

namespace nx {
namespace gstreamer {

Pad::Pad(GstPad* pad):
    m_nativePad(pad, [](GstPad* pad){ gst_object_unref(GST_OBJECT(pad)); })
{
}

PadProbeId Pad::addProbe(ProbeFunc func, GstPadProbeType probeType, gpointer userData)
{
    return gst_pad_add_probe(
        m_nativePad.get(),
        probeType,
        func,
        userData,
        NULL);
}

void Pad::removeProbe(PadProbeId probeId)
{
    gst_pad_remove_probe(m_nativePad.get(), probeId);
}

GstPad* Pad::nativePad() const
{
    return m_nativePad.get();
}

} // namespace gstreamer
} // namespace nx
