// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <nx/sdk/interface.h>

#include "i_data_packet.h"
#include "i_device_agent.h"

namespace nx {
namespace sdk {
namespace analytics {

/**
 * Interface for a DeviceAgent that requires input (e.g. audio or video stream) from the Engine
 * plugin container.
 */
class IConsumingDeviceAgent: public Interface<IConsumingDeviceAgent, IDeviceAgent>
{
public:
    static auto interfaceId() { return InterfaceId("nx::sdk::analytics::IConsumingDeviceAgent"); }

    /**
     * Supplies data to the engine. Called from a worker thread.
     *
     * @param dataPacket Never null. Has a valid timestamp >= 0.
     */
    protected: virtual void doPushDataPacket(Result<void>* outResult, IDataPacket* dataPacket) = 0;
    public: Result<void> pushDataPacket(IDataPacket* dataPacket)
    {
        Result<void> result;
        doPushDataPacket(&result, dataPacket);
        return result;
    }
};

} // namespace analytics
} // namespace sdk
} // namespace nx
