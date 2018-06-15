#pragma once

#include "data.h"

#include <chrono>

#include <QtCore/QtGlobal>

#include <nx/utils/uuid.h>
#include <nx/vms/api/types/connection_types.h>

namespace nx {
namespace vms {
namespace api {

struct NX_VMS_API TimeData: Data
{
    TimeData() = default;
    TimeData(qint64 valueMs): value(valueMs) {}
    TimeData(std::chrono::milliseconds value): TimeData(value.count()) {}

    qint64 value = 0; //< ms.
    bool isPrimaryTimeServer = false;
    QnUuid primaryTimeServerGuid;
    TimeFlags syncTimeFlags = TimeFlag::none;
};
#define TimeData_Fields \
    (value) \
    (isPrimaryTimeServer) \
    (primaryTimeServerGuid) \
    (syncTimeFlags)

} // namespace api
} // namespace vms
} // namespace nx
