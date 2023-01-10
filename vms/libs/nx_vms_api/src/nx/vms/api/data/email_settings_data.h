// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <QtCore/QString>

#include <nx/vms/api/types/smtp_types.h>

#include "data_macros.h"

namespace nx {
namespace vms {
namespace api {

struct NX_VMS_API EmailSettingsData
{
    QString host;
    int port = 0;
    QString user;
    QString from;
    QString password;
    ConnectionType connectionType = ConnectionType::unsecure;
};
#define EmailSettingsData_Fields (host)(port)(user)(from)(password)(connectionType)
NX_VMS_API_DECLARE_STRUCT(EmailSettingsData)

} // namespace api
} // namespace vms
} // namespace nx

