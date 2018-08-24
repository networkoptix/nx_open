#pragma once

#include <memory>
#include <functional>
#include <cstdint>

extern "C" {

#include <gst/gst.h>

} // extern "C"

namespace nx {
namespace gstreamer {

using NativeMessagePtr = std::unique_ptr<GstMessage, std::function<void(GstMessage*)>>;

class BusMessage
{
public:
    explicit BusMessage(GstMessage* message);
    virtual ~BusMessage();
    BusMessage(BusMessage&& other);

    BusMessage(const BusMessage&) = delete;
    BusMessage& operator=(const BusMessage&) = delete;

    GstMessageType type() const;
    uint64_t timestamp() const;
    uint32_t sequenceNumber() const;

    GstMessage* nativeMessage() const;

    void release();

protected:
    NativeMessagePtr m_message = nullptr;

private:
    bool m_released = false;
};

} // namespace gstreamer
} // namespace nx
