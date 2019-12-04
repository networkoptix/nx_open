#include "utils.h"

#include <common/common_module.h>

#include <nx/utils/log/log.h>

#include <nx/vms/server/root_fs.h>

#include <nx/vms/server/nvr/hanwha/model_info.h>
#include <nx/vms/server/nvr/hanwha/ioctl_common.h>

namespace nx::vms::server::nvr::hanwha {

std::optional<DeviceInfo> getDeviceInfo(RootFileSystem* rootFileSystem)
{
#if defined (Q_OS_LINUX)
    const int powerSupplyDeviceDescriptor = rootFileSystem->open(
        kPowerSupplyDeviceFileName,
        QIODevice::ReadWrite);

    if (powerSupplyDeviceDescriptor < 0)
    {
        NX_DEBUG(NX_SCOPE_TAG, "Unable to open device '%1'", kPowerSupplyDeviceFileName);
        return std::nullopt;
    }

    const int command = prepareCommand<int>(kGetBoardIdCommand);

    int boardId = -1;
    if (const int result = ::ioctl(powerSupplyDeviceDescriptor, command, &boardId); result != 0)
    {
        NX_WARNING(NX_SCOPE_TAG, "Unable to get board id, result %1", result);
        return std::nullopt;
    }

    return DeviceInfo{"Hanwha", getModelByBoardId(boardId)};
#else
    return std::nullopt;
#endif
}

} // namespace nx::vms::server::nvr::hanwha
