#pragma once

#include "data_packet.h"
#include "camera_manager.h"

namespace nx {
namespace sdk {
namespace metadata {

/**
 * Each class that implements ConsumingCameraManager interface
 * should properly handle this GUID in its queryInterface method
 */
static const nxpl::NX_GUID IID_ConsumingCameraManager =
    {{0xb6, 0x7b, 0xce, 0x8c, 0x67, 0x68, 0x4e, 0x9b, 0xbd, 0x56, 0x8c, 0x02, 0xca, 0x0a, 0x1b, 0x18}};

/**
 * @brief The ConsumingCameraManager class is an interface for
 * metadata manager that requires input (e.g. audio or video stream) from the
 * plugin container.
 */
class ConsumingCameraManager: public CameraManager
{
public:
    /**
     * @brief pushDataPacket provides data to the plugin.
     * @param dataPacket packet of data.
     * @return noError in case of success, other value in case of failure.
     */
    virtual Error pushDataPacket(DataPacket* dataPacket) = 0;
};

} // namespace metadata
} // namespace sdk
} // namespace nx
