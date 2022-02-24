// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include "resource_data.h"

#include <QtCore/QString>

#include <nx/vms/api/data/user_data.h>

namespace nx::vms::api {

struct NX_VMS_API UserDataEx: UserData
{
    QString password; /**<%apidoc[opt] */

    bool operator==(const UserDataEx& other) const = default;
};
#define UserDataEx_Fields UserData_Fields (password)
NX_VMS_API_DECLARE_STRUCT(UserDataEx)

} // namespace nx::vms::api

Q_DECLARE_METATYPE(nx::vms::api::UserDataEx)
