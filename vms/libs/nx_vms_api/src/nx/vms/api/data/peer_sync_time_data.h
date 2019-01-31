#pragma once

#include "data.h"

#include <QtCore/QtGlobal>

#include <nx/utils/uuid.h>

namespace nx {
namespace vms {
namespace api {

struct NX_VMS_API PeerSyncTimeData: public Data
{
    qint64 syncTimeMs = 0;
};
#define PeerSyncTimeData_Fields (syncTimeMs)

} // namespace api
} // namespace vms
} // namespace nx

Q_DECLARE_METATYPE(nx::vms::api::PeerSyncTimeData)
