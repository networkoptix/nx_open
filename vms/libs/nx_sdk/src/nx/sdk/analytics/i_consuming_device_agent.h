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
     * @param dataPacket Never null. Has a valid timestamp >= 0.
     * @param outError Output parameter for error reporting. Never null. Must contain
     *     `ErrorCode::noError` error code in the case of success (`outError` object is guarnteed to
     *     be prefilled with `noError` value, so no additional actions are required) or be properly
     *     filled in a case of failure.
     */
    virtual void pushDataPacket(IDataPacket* dataPacket, IError* outError) = 0;
};

} // namespace analytics
} // namespace sdk
} // namespace nx
