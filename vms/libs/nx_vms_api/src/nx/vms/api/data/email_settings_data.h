#pragma once

#include "data.h"

#include <QtCore/QString>

#include <nx/vms/api/types/smtp_types.h>

namespace nx {
namespace vms {
namespace api {

struct NX_VMS_API EmailSettingsData: Data
{
    QString host;
    int port = 0;
    QString user;
    QString from;
    QString password;
    ConnectionType connectionType = ConnectionType::unsecure;
};
#define EmailSettingsData_Fields (host)(port)(user)(from)(password)(connectionType)

} // namespace api
} // namespace vms
} // namespace nx

Q_DECLARE_METATYPE(nx::vms::api::EmailSettingsData)
