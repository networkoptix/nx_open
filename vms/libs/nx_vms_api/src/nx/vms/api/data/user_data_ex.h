#pragma once

#include "resource_data.h"

#include <QtCore/QString>

#include <nx/vms/api/data/user_data.h>

namespace nx::vms::api {

struct NX_VMS_API UserDataEx : UserData
{
    QString password;
};

#define UserDataEx_Fields UserData_Fields (password)

} // namespace nx::vms::api

Q_DECLARE_METATYPE(nx::vms::api::UserDataEx)
