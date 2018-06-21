#pragma once

#include "data.h"
#include "timestamp.h"

#include <QtCore/QtGlobal>

#include <nx/utils/uuid.h>

namespace nx {
namespace vms {
namespace api {

struct NX_VMS_API SystemIdData: Data
{
    SystemIdData() = default;
    SystemIdData(const QString& systemId): systemId(systemId) {}

    QnUuid systemId;
    qint64 sysIdTime = 0;
    Timestamp tranLogTime;
};
#define SystemIdData_Fields (systemId)(sysIdTime)(tranLogTime)

} // namespace api
} // namespace vms
} // namespace nx

Q_DECLARE_METATYPE(nx::vms::api::SystemIdData)
