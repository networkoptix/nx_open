#pragma once

#include "data.h"

#include <QtCore/QtGlobal>

namespace nx {
namespace vms {
namespace api {

struct NX_VMS_API VideoWallLicenseOverflowData: Data
{
    bool value = false;
    qint64 time = 0;
};
#define VideoWallLicenseOverflowData_Fields (value)(time)

} // namespace api
} // namespace vms
} // namespace nx

Q_DECLARE_METATYPE(nx::vms::api::VideoWallLicenseOverflowData)
