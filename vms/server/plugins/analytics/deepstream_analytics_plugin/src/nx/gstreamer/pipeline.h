#pragma once

#include <functional>

#include <nx/gstreamer/bin.h>
#include <nx/gstreamer/bus.h>
#include <nx/gstreamer/bus_message.h>

#include <nx/sdk/analytics/i_metadata_packet.h>
#include <nx/sdk/analytics/i_data_packet.h>

namespace nx {
namespace gstreamer {

class Pipeline;

using ObjectPacketHandler = void(nx::sdk::analytics::IMetadataPacket* packet);
using MetadataCallback = std::function<ObjectPacketHandler>;
using BusCallback = std::function<bool(Bus* bus, BusMessage* message, Pipeline* pipeline)>;

class Pipeline: public Bin
{
    using base_type = Bin;
public:
    Pipeline(const ElementName& pipelineName);
    virtual ~Pipeline();

    Pipeline(const Pipeline&) = delete;
    Pipeline& operator=(const Pipeline&) = delete;
    Pipeline(Pipeline&& other);

    void setBusCallback(BusCallback busCallback);

    bool handleBusMessage(BusMessage* message);

    virtual void setMetadataCallback(MetadataCallback MetadataCallback) = 0;

    virtual bool pushDataPacket(nx::sdk::analytics::IDataPacket* dataPacket) = 0;

    virtual bool start() = 0;

    virtual bool stop() = 0;

    virtual bool pause() = 0;

    virtual GstState state() const = 0;
};

} // namespace gstreamer
} // namespace nx
