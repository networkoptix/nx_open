#include "bus_message.h"

namespace nx {
namespace gstreamer {

BusMessage::BusMessage(GstMessage* message)
{
    gst_message_ref(message);
    m_message = NativeMessagePtr(
        message,
        [](GstMessage* message) { gst_message_unref(message); });
}

BusMessage::~BusMessage()
{
    if (m_released)
        m_message.release();
}

BusMessage::BusMessage(BusMessage&& other):
    m_message(std::move(other.m_message)),
    m_released(other.m_released)
{
    other.m_message = nullptr;
    other.m_released = true;
}

GstMessageType BusMessage::type() const
{
    return GST_MESSAGE_TYPE(m_message.get());
}

uint64_t BusMessage::timestamp() const
{
    return static_cast<uint64_t>(GST_MESSAGE_TIMESTAMP(m_message.get()));
}

uint32_t BusMessage::sequenceNumber() const
{
    return static_cast<uint32_t>(GST_MESSAGE_SEQNUM(m_message.get()));
}

GstMessage* BusMessage::nativeMessage() const
{
    return m_message.get();
}

void BusMessage::release()
{
    m_released = true;
}

} // namespace gstreamer
} // namespace nx
