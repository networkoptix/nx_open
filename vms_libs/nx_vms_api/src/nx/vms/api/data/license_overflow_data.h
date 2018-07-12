#pragma once

#include "data.h"

#include <QtCore/QtGlobal>

namespace nx {
namespace vms {
namespace api {

struct NX_VMS_API LicenseOverflowData: Data
{
    bool value = false;
    qint64 time = 0;
};
#define LicenseOverflowData_Fields (value)(time)

} // namespace api
} // namespace vms
} // namespace nx

Q_DECLARE_METATYPE(nx::vms::api::LicenseOverflowData)
