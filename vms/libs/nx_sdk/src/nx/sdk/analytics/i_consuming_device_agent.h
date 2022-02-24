// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <nx/sdk/interface.h>

#include "i_data_packet.h"
#include "i_device_agent.h"

namespace nx {
namespace sdk {
namespace analytics {

/**
 * Interface for a DeviceAgent that requires input (e.g. audio or video stream) from the Device.
 */
class IConsumingDeviceAgent0: public Interface<IConsumingDeviceAgent0, IDeviceAgent0>
{
public:
    static auto interfaceId() { return makeId("nx::sdk::analytics::IConsumingDeviceAgent"); }

    /** Called by pushDataPacket() */
    protected: virtual void doPushDataPacket(Result<void>* outResult, IDataPacket* dataPacket) = 0;
    /**
     * Supplies data to the Engine. Called from a worker thread.
     *
     * @param dataPacket Never null. Has a valid timestamp >= 0.
     */
    public: Result<void> pushDataPacket(IDataPacket* dataPacket)
    {
        Result<void> result;
        doPushDataPacket(&result, dataPacket);
        return result;
    }
};

class IConsumingDeviceAgent: public Interface<IConsumingDeviceAgent, IConsumingDeviceAgent0>
{
public:
    static auto interfaceId() { return makeId("nx::sdk::analytics::IConsumingDeviceAgent1"); }

    /**
     * Called by the Server when this DeviceAgent is no longer needed, before releasing the ref.
     */
    virtual void finalize() = 0;
};
using IConsumingDeviceAgent1 = IConsumingDeviceAgent;

} // namespace analytics
} // namespace sdk
} // namespace nx
