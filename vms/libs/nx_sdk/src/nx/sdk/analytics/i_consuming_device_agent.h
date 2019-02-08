#pragma once

#include "i_data_packet.h"
#include "i_device_agent.h"

namespace nx {
namespace sdk {
namespace analytics {

/**
 * Each class that implements IConsumingDeviceAgent interface should properly handle this GUID in
 * its queryInterface().
 */
static const nxpl::NX_GUID IID_ConsumingDeviceAgent =
    {{0xb6,0x7b,0xce,0x8c,0x67,0x68,0x4e,0x9b,0xbd,0x56,0x8c,0x02,0xca,0x0a,0x1b,0x18}};

/**
 * Interface for a DeviceAgent that requires input (e.g. audio or video stream) from the Engine
 * plugin container.
 */
class IConsumingDeviceAgent: public IDeviceAgent
{
public:
    /**
     * Supplies data to the engine. Called from a worker thread.
     * @param dataPacket Never null. Has a valid timestamp >= 0.
     * @return noError in case of success, other value in case of failure.
     */
    virtual Error pushDataPacket(IDataPacket* dataPacket) = 0;
};

} // namespace analytics
} // namespace sdk
} // namespace nx
